#pragma once

#include <vulkan/vulkan.hpp>

#include <functional>
#include <optional>
#include <variant>

struct PhysicalDeviceInfo
{
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkExtensionProperties> extensionProperties;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
};

struct QueueFamilyInfo
{
    uint32_t index;
    VkQueueFamilyProperties properties;
};

class DeviceBuilder
{
  public:
    enum class ErrorType
    {
        None = 0,
        EnumeratePhysicalDevices,
        NoPhysicalDeviceFound,
        NoQueueFamilyFound,
        DeviceCreationError,
        NoSurfaceFormatFound,
        NoPresentModeFound,
        SwapChainCreationError,
        Fatal, // Something unspecified happened but we can't continue
    };

    struct Error
    {
        ErrorType type;
        union
        {
            struct
            {
                VkResult result;
            } EnumeratePhysicalDevices;
            struct
            {
                VkResult result;
            } DeviceCreationError;
            struct
            {
                VkResult result;
            } SwapChainCreationError;
            struct
            {
                VkResult result;
                const char* message;
            } Fatal;
        };
    };

    struct Data
    {
        VkDevice device;
        VkSwapchainKHR swapChain;
        PhysicalDeviceInfo physicalDeviceInfo;
        QueueFamilyInfo queueFamilyProperties;
    };

    // If an error occurs, nothing should be returned. This is mainly supported because some
    // functions like vkGetPhysicalDeviceSurfaceSupportKHR might return an error
    using DeviceSelector = std::function<std::variant<bool, VkResult>(
        const std::optional<PhysicalDeviceInfo>&,
        const PhysicalDeviceInfo&)>;
    using QueueFamilySelector = std::function<std::variant<bool, VkResult>(
        const std::optional<QueueFamilyInfo>&,
        const QueueFamilyInfo&)>;
    using SurfaceFormatSelector =
        std::function<std::optional<VkSurfaceFormatKHR>(const PhysicalDeviceInfo&)>;
    using PresentModeSelector =
        std::function<std::optional<VkPresentModeKHR>(const PhysicalDeviceInfo&)>;
    using BuildType = std::variant<Data, Error>;

    DeviceBuilder(const VkInstance instance, const VkSurfaceKHR surface);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);
    DeviceBuilder& selectSurfaceFormat(SurfaceFormatSelector selector);
    DeviceBuilder& selectPresentMode(PresentModeSelector selector);

    DeviceBuilder& withRequiredExtension(const char* name);

    BuildType build();

  private:
    const VkInstance instance;
    const VkSurfaceKHR surface;

    std::vector<const char*> requiredExtensions;

    DeviceSelector deviceSelector;
    QueueFamilySelector queueFamilySelector;
    SurfaceFormatSelector surfaceFormatSelector;
    PresentModeSelector presentModeSelector;

    std::optional<PhysicalDeviceInfo> deviceOpt;
    std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};

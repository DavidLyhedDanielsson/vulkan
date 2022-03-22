#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <functional>
#include <optional>
#include <variant>

struct PhysicalDeviceInfo
{
    vk::PhysicalDevice device;
    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceFeatures features;
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties;
    std::vector<vk::ExtensionProperties> extensionProperties;
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    std::vector<vk::PresentModeKHR> presentModes;
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
};

struct QueueFamilyInfo
{
    uint32_t index;
    vk::QueueFamilyProperties properties;
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
        NoSurfaceFormatFound, // These "NoXFound" can probably be under a "out of memory" umbrella
        NoPresentModeFound,
        NoSurfaceCapabilitiesFound,
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
                vk::Result result;
            } EnumeratePhysicalDevices;
            struct
            {
                vk::Result result;
            } DeviceCreationError;
            struct
            {
                vk::Result result;
            } NoSurfaceFormatFound;
            struct
            {
                vk::Result result;
            } NoPresentModeFound;
            struct
            {
                vk::Result result;
            } NoSurfaceCapabilitiesFound;
            struct
            {
                vk::Result result;
            } SwapChainCreationError;
            struct
            {
                vk::Result result;
                const char* message;
            } Fatal;
        };
    };

    struct Data
    {
        vk::UniqueDevice device;
        vk::UniqueSwapchainKHR swapChain;
        PhysicalDeviceInfo physicalDeviceInfo;
        QueueFamilyInfo queueFamilyProperties;
    };

    // If an error occurs, nothing should be returned. This is mainly supported because some
    // functions like vkGetPhysicalDeviceSurfaceSupportKHR might return an error
    using DeviceSelector = std::function<std::variant<bool, vk::Result>(
        const std::optional<PhysicalDeviceInfo>&,
        const PhysicalDeviceInfo&)>;
    using QueueFamilySelector = std::function<std::variant<bool, vk::Result>(
        const std::optional<QueueFamilyInfo>&,
        const QueueFamilyInfo&)>;
    using SurfaceFormatSelector =
        std::function<std::optional<vk::SurfaceFormatKHR>(const PhysicalDeviceInfo&)>;
    using PresentModeSelector =
        std::function<std::optional<vk::PresentModeKHR>(const PhysicalDeviceInfo&)>;
    using BuildType = std::variant<Data, Error>;

    DeviceBuilder(const vk::UniqueInstance& instance, const vk::UniqueSurfaceKHR& surface);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);
    DeviceBuilder& selectSurfaceFormat(SurfaceFormatSelector selector);
    DeviceBuilder& selectPresentMode(PresentModeSelector selector);

    DeviceBuilder& withRequiredExtension(const char* name);

    BuildType build();

  private:
    const vk::UniqueInstance& instance;
    const vk::UniqueSurfaceKHR& surface;

    std::vector<const char*> requiredExtensions;

    DeviceSelector deviceSelector;
    QueueFamilySelector queueFamilySelector;
    SurfaceFormatSelector surfaceFormatSelector;
    PresentModeSelector presentModeSelector;

    std::optional<PhysicalDeviceInfo> deviceOpt;
    std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};

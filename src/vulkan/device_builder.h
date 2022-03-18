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
                const char* message;
            } Fatal;
        };
    };

    struct Data
    {
        VkDevice device;
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
    using BuildType = std::variant<Data, Error>;

    DeviceBuilder(const VkInstance instance, const VkSurfaceKHR surface);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);

    BuildType build();

  private:
    const VkInstance instance;
    const VkSurfaceKHR surface;

    DeviceSelector deviceSelector;
    QueueFamilySelector queueFamilySelector;

    std::optional<PhysicalDeviceInfo> deviceOpt;
    std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};

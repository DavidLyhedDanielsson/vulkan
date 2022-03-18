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
        };
    };

    struct Data
    {
        VkDevice device;
        PhysicalDeviceInfo physicalDeviceInfo;
        QueueFamilyInfo queueFamilyProperties;
    };

    using DeviceSelector =
        std::function<bool(const std::optional<PhysicalDeviceInfo>&, const PhysicalDeviceInfo&)>;
    using QueueFamilySelector =
        std::function<bool(const std::optional<QueueFamilyInfo>&, const QueueFamilyInfo&)>;
    using BuildType = std::variant<Data, Error>;

    DeviceBuilder(const VkInstance instance, const VkSurfaceKHR surface);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);

    BuildType build();

  private:
    const VkInstance instance;
    const VkSurfaceKHR surface;

    std::function<bool(const std::optional<PhysicalDeviceInfo>&, const PhysicalDeviceInfo&)>
        deviceSelector;
    std::function<bool(const std::optional<QueueFamilyInfo>&, const QueueFamilyInfo&)>
        queueFamilySelector;

    std::optional<PhysicalDeviceInfo> deviceOpt;
    std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};

#pragma once

#include <vulkan/vulkan.hpp>

#include <functional>
#include <optional>
#include <variant>

struct DeviceError
{
    // TODO: Fill with error info
};

struct DeviceInfo
{
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
};

struct QueueFamilyInfo
{
    uint32_t index;
    VkQueueFamilyProperties properties;
};

class DeviceBuilder
{
  public:
    using DeviceSelector = std::function<bool(const std::optional<DeviceInfo>&, const DeviceInfo&)>;
    using QueueFamilySelector =
        std::function<bool(const std::optional<QueueFamilyInfo>&, const QueueFamilyInfo&)>;
    using BuildType = std::variant<VkDevice, DeviceError>;

    DeviceBuilder(const VkInstance instance);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);

    BuildType build();

  private:
    const VkInstance instance;

    std::function<bool(const std::optional<DeviceInfo>&, const DeviceInfo&)> deviceSelector;
    std::function<bool(const std::optional<QueueFamilyInfo>&, const QueueFamilyInfo&)>
        queueFamilySelector;

    std::optional<DeviceInfo> deviceOpt;
    std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};

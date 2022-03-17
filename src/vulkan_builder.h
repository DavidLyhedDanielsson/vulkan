#pragma once

// TODO: Separate this file, probably. Maybe?

#include <vulkan/vulkan.hpp>

#include <functional>
#include <optional>
#include <variant>
#include <vector>

struct Layer
{
    const char* name;
    bool required;
};

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

struct InstanceError
{
    // TODO: Fill with error info
};

class InstanceBuilder
{
  public:
    using BuildType = std::variant<VkInstance, InstanceError>;

    InstanceBuilder();
    InstanceBuilder& withApplicationVersion(
        const char* name,
        short major,
        short minor,
        short patch);
    InstanceBuilder& withEngine(const char* name, short major, short minor, short patch);
    InstanceBuilder& withVulkanVersion(uint32_t version);
    InstanceBuilder& withRequiredLayer(const char* name);
    InstanceBuilder& withRequiredExtension(const char* name);
    InstanceBuilder& withRequiredExtensions(const char** names, uint32_t count);

    InstanceBuilder& withValidationLayer();
    InstanceBuilder& withDebugExtension();

    BuildType build();

  private:
    VkApplicationInfo applicationInfo;
    std::vector<Layer> layers;
    std::vector<const char*> requiredExtensions;
};
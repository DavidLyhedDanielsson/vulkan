#pragma once

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
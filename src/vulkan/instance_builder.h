#pragma once

#include "../config.h"
#include <vulkan/vulkan.hpp>

#include <functional>
#include <optional>
#include <vector>

struct Layer
{
    const char* name;
    bool required;
};

class InstanceBuilder
{
  public:
    enum class ErrorType
    {
        None = 0,
        RequiredLayerMissing,
        InstanceCreationError,
        EnumerateInstanceLayerProperties,
    };

    struct Error
    {
        ErrorType type;
        union
        {
            struct
            {
                const char* layers[16];
                uint32_t count;
            } RequiredLayerMissing;
            struct
            {
                vk::Result result;
            } InstanceCreationError;
            struct
            {
                vk::Result result;
            } EnumerateInstanceLayerProperties;
        };
    };

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

    std::optional<Error> build(SelectedConfig&);

  private:
    vk::ApplicationInfo applicationInfo;
    std::vector<Layer> layers;
    std::vector<const char*> requiredExtensions;
};
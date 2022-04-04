#include "instance_builder.h"

#include "../stl_utils.h"
#include <variant>

struct ValidatedLayer
{
    const char* name;
    bool required;
    bool found;
};

vk::Result visitValidationLayers(const std::function<void(vk::LayerProperties)>& visitor)
{
    auto [res, layers] = vk::enumerateInstanceLayerProperties();
    if(res != vk::Result::eSuccess)
        return res;

    for(auto layer : layers)
        visitor(layer);

    return vk::Result::eSuccess;
}

std::variant<std::vector<ValidatedLayer>, vk::Result> validateLayers(
    const std::vector<Layer>& layers)
{
    std::vector<ValidatedLayer> validatedLayers = map(layers, [](const Layer& layer) {
        return ValidatedLayer{.name = layer.name, .required = layer.required, .found = false};
    });

    auto result = visitValidationLayers([&validatedLayers, layers](vk::LayerProperties prop) {
        for(auto [layer, index] : IndexRef(layers))
        {
            if(std::strcmp(layer.name, prop.layerName) == 0)
            {
                validatedLayers[index].found = true;
                return;
            }
        }
    });

    if(result != vk::Result::eSuccess)
        return {result};
    else
        return {validatedLayers};
}

InstanceBuilder::InstanceBuilder()
    : applicationInfo({
        .pApplicationName = "Vulkan application",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName = "Undecided",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0,
    })
{
}

InstanceBuilder& InstanceBuilder::withApplicationVersion(
    const char* name,
    short major,
    short minor,
    short patch)
{
    applicationInfo.pApplicationName = name;
    applicationInfo.apiVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
    return *this;
}

InstanceBuilder& InstanceBuilder::withEngine(
    const char* name,
    short major,
    short minor,
    short patch)
{
    applicationInfo.pEngineName = name;
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
    return *this;
}

InstanceBuilder& InstanceBuilder::withVulkanVersion(uint32_t version)
{
    applicationInfo.apiVersion = version;
    return *this;
}

InstanceBuilder& InstanceBuilder::withRequiredLayer(const char* name)
{
    layers.push_back(Layer{.name = name, .required = true});
    return *this;
}

InstanceBuilder& InstanceBuilder::withRequiredExtension(const char* name)
{
    requiredExtensions.push_back(name);
    return *this;
}

InstanceBuilder& InstanceBuilder::withRequiredExtensions(const char** names, uint32_t count)
{
    for(uint32_t i = 0; i < count; ++i)
        requiredExtensions.push_back(names[i]);
    return *this;
}

InstanceBuilder& InstanceBuilder::withValidationLayer()
{
    layers.push_back(Layer{.name = "VK_LAYER_KHRONOS_validation", .required = true});
    return *this;
}

InstanceBuilder& InstanceBuilder::withDebugExtension()
{
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return *this;
}

std::optional<InstanceBuilder::Error> InstanceBuilder::build(SelectedConfig& config)
{
    // Some errors require multiple iterations in loops and such, so declare it here
    Error error = {};

    auto validatedLayersVar = validateLayers(layers);
    if(std::holds_alternative<vk::Result>(validatedLayersVar))
    {
        error.type = ErrorType::EnumerateInstanceLayerProperties;
        error.EnumerateInstanceLayerProperties.result = std::get<vk::Result>(validatedLayersVar);
        return error;
    }

    auto validatedLayers = std::get<std::vector<ValidatedLayer>>(validatedLayersVar);
    for(const ValidatedLayer& layer : validatedLayers)
    {
        if(layer.required && !layer.found)
        {
            error.type = ErrorType::RequiredLayerMissing;
            error.RequiredLayerMissing.layers[error.RequiredLayerMissing.count++] = layer.name;
        }
    }
    if(error.type != ErrorType::None)
        return error;

    std::vector<const char*> layerNames =
        map(layers, [](const Layer& layer) { return layer.name; });

    vk::InstanceCreateInfo instanceCreateInfo = {
        .pApplicationInfo = &this->applicationInfo,
        .enabledLayerCount = (uint32_t)layerNames.size(),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    auto [res, instance] = vk::createInstanceUnique(instanceCreateInfo);
    if(res != vk::Result::eSuccess)
    {
        error.type = ErrorType::InstanceCreationError;
        error.InstanceCreationError.result = res;
        return error;
    }

    config.instance = std::move(instance);

    return std::nullopt;
}

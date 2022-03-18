#include "instance_builder.h"

#include "../stl_utils.h"

struct ValidatedLayer
{
    const char* name;
    bool required;
    bool found;
};

void visitValidationLayers(std::function<void(VkLayerProperties)> visitor)
{
    uint32_t writtenProperties = 0;
    vkEnumerateInstanceLayerProperties(&writtenProperties, nullptr);

    // I wanted to do this without dynamic allocation but apparently there's no
    // way to specify an offset into vkEnumerate
    std::vector<VkLayerProperties> layers(writtenProperties);
    auto res = vkEnumerateInstanceLayerProperties(&writtenProperties, layers.data());
    assert(res == VK_SUCCESS);

    for(uint32_t i = 0; i < writtenProperties; ++i)
        visitor(layers[i]);
}

std::vector<ValidatedLayer> validateLayers(const std::vector<Layer>& layers)
{
    std::vector<ValidatedLayer> validatedLayers = map(layers, [](const Layer& layer) {
        return ValidatedLayer{.name = layer.name, .required = layer.required, .found = false};
    });

    visitValidationLayers([&validatedLayers, layers](VkLayerProperties prop) {
        for(auto [layer, index] : Index(layers))
        {
            if(std::strcmp(layer.name, prop.layerName) == 0)
            {
                validatedLayers[index].found = true;
                return;
            }
        }
    });

    return validatedLayers;
}

InstanceBuilder::InstanceBuilder()
    : applicationInfo({
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
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

InstanceBuilder::BuildType InstanceBuilder::build()
{
    for(const ValidatedLayer& layer : validateLayers(layers))
    {
        if(layer.required && !layer.found)
            return BuildType(InstanceError{});
    }

    std::vector<const char*> layerNames =
        map(layers, [](const Layer& layer) { return layer.name; });

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &this->applicationInfo,
        .enabledLayerCount = (uint32_t)layerNames.size(),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    VkInstance instance;
    auto retVal = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if(retVal == VK_SUCCESS)
        return BuildType(instance);
    else
        return BuildType(InstanceError{});
}

#include "vulkan.h"
#include <GLFW/glfw3.h>
#include <array>
#include <vector>

#include "stl_utils.h"

#define vkGetInstanceProcAddrQ(instance, func) (PFN_##func) vkGetInstanceProcAddr(instance, #func)

struct Layer
{
    const char* name;
    bool required;
};

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
    assert(res == VK_SUCCESS || res == VK_INCOMPLETE);

    for(uint32_t i = 0; i < writtenProperties; ++i)
        visitor(layers[i]);
}

std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // This could currently use std::array, but for future proofing (runtime conditional inclusion)
    // it's an std::array, same with getRequiredLayers
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

std::vector<Layer> getRequiredLayers()
{
    // At some point this will be configurable
    return std::vector{Layer{.name = "VK_LAYER_KHRONOS_validation", .required = false}};
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

std::optional<Vulkan> Vulkan::createVulkan()
{
    // TODO: Actual error handling

    std::vector<Layer> requiredLayers = getRequiredLayers();
    for(const ValidatedLayer& layer : validateLayers(requiredLayers))
    {
        if(layer.required && !layer.found)
            return std::nullopt;
    }

    std::vector<const char*> requiredLayerNames =
        map(requiredLayers, [](const Layer& layer) { return layer.name; });
    std::vector<const char*> requiredExtensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &Vulkan::APP_INFO,
        .enabledLayerCount = (uint32_t)requiredLayerNames.size(),
        .ppEnabledLayerNames = requiredLayerNames.data(),
        .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    VkInstance instance;
    auto retVal = vkCreateInstance(&createInfo, nullptr, &instance);
    if(retVal == VK_SUCCESS)
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = Vulkan::debugCallback,
            .pUserData = nullptr,
        };

        Vulkan vulkan(instance);

        auto f = vkGetInstanceProcAddrQ(instance, vkCreateDebugUtilsMessengerEXT);
        assert(f);
        assert(f(instance, &createInfo, nullptr, &(vulkan.msg)) == VK_SUCCESS);

        return vulkan;
    }
    else
    {
        return std::nullopt;
    }
}

Vulkan::Vulkan(VkInstance instance)
    : instance(instance, [](VkInstance_T* instance) { vkDestroyInstance(instance, nullptr); })
{
}

Vulkan::~Vulkan()
{
    // Moving an instance is allowed
    if(instance.get())
    {
        if(auto destroy = vkGetInstanceProcAddrQ(instance.get(), vkDestroyDebugUtilsMessengerEXT);
           destroy)
        {
            destroy(instance.get(), msg, nullptr);
        }
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    return VK_FALSE;
}
#include "vulkan.h"
#include <GLFW/glfw3.h>
#include <array>
#include <vector>

#include "vulkan_builder.h"

#include "stl_utils.h"

#define vkGetInstanceProcAddrQ(instance, func) (PFN_##func) vkGetInstanceProcAddr(instance, #func)

std::optional<Vulkan> Vulkan::createVulkan()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto instance =
        std::get<VkInstance>(InstanceBuilder()
                                 .withValidationLayer()
                                 .withDebugExtension()
                                 .withRequiredExtensions(glfwExtensions, glfwExtensionCount)
                                 .build());

    auto physicalDevice = std::get<VkDevice>(
        DeviceBuilder(instance)
            .selectDevice([](auto current, auto potential) {
                // Select any GPU but prioritize discrete GPUs
                // clang-format creates some serious stairs here :(
                bool hasDiscrete = current.has_value()
                                   && current.value().properties.deviceType
                                          == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

                return !hasDiscrete
                       && (potential.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                           || potential.properties.deviceType
                                  == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
            })
            .selectQueueFamily([](auto current, auto potential) {
                return !current.has_value()
                       && potential.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
            })
            .build());

    VkDebugUtilsMessengerCreateInfoEXT msgCreateInfo = {
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

    Vulkan vulkan(instance, physicalDevice);

    auto f = vkGetInstanceProcAddrQ(instance, vkCreateDebugUtilsMessengerEXT);
    assert(f);
    assert(f(instance, &msgCreateInfo, nullptr, &(vulkan.msg)) == VK_SUCCESS);

    return vulkan;
}

Vulkan::Vulkan(VkInstance instance, VkDevice device)
    : instance(instance, [](VkInstance_T* instance) { vkDestroyInstance(instance, nullptr); })
    , device(device, [](VkDevice_T* device) { vkDestroyDevice(device, nullptr); })
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
#include <GLFW/glfw3.h>

#include "vulkan.h"
#include <array>
#include <vector>

#include "../stl_utils.h"

#include "device_builder.h"
#include "instance_builder.h"

#define vkGetInstanceProcAddrQ(instance, func) (PFN_##func) vkGetInstanceProcAddr(instance, #func)

Vulkan::CreateType Vulkan::createVulkan(GLFWwindow* windowHandle)
{
    Error error = {};

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto instanceVar = InstanceBuilder()
                           .withValidationLayer()
                           .withDebugExtension()
                           .withRequiredExtensions(glfwExtensions, glfwExtensionCount)
                           .build();

    if(std::holds_alternative<InstanceBuilder::Error>(instanceVar))
    {
        // TODO: Log error; there is no possibility of user error here (yet)
        auto err = std::get<InstanceBuilder::Error>(instanceVar);
        if(err.type == InstanceBuilder::ErrorType::RequiredLayerMissing)
            error.type = ErrorType::ConfigError;
        else
            error.type = ErrorType::HardwareError;
        return error;
    }
    auto instance = std::get<VkInstancePtr>(std::move(instanceVar));

    VkSurfaceKHR surfaceRaw;
    auto res = glfwCreateWindowSurface(instance, windowHandle, nullptr, &surfaceRaw);
    VkSurfacePtr surface(instance, surfaceRaw);
    if(res != VK_SUCCESS)
    {
        error.type = ErrorType::Unsupported;
        error.Unsupported.message = "Can't create surface";
        return error;
    }

    // Defaults are fine for now
    auto deviceVar = DeviceBuilder(instance, surface).build();

    if(std::holds_alternative<DeviceBuilder::Error>(deviceVar))
    {
        // TODO: Log error; there is no possibility of user error here (yet)
        error.type = ErrorType::HardwareError;
        return error;
    }
    auto deviceInfo = std::get<DeviceBuilder::Data>(std::move(deviceVar));

    VkQueue workQueue;
    vkGetDeviceQueue(deviceInfo.device, deviceInfo.queueFamilyProperties.index, 0, &workQueue);

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

    Vulkan vulkan(
        std::move(instance),
        std::move(deviceInfo.device),
        workQueue,
        std::move(surface),
        std::move(deviceInfo.swapChain));

    auto createDebugUtilsMessenger =
        vkGetInstanceProcAddrQ(vulkan.instance, vkCreateDebugUtilsMessengerEXT);
    if(!createDebugUtilsMessenger)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrNotFound.instanceProc = "vkCreateDebugUtilsMessengerEXT";
        return error;
    }
    res = createDebugUtilsMessenger(vulkan.instance, &msgCreateInfo, nullptr, &(vulkan.msg));
    if(res != VK_SUCCESS)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrError.instanceProc = "vkCreateDebugUtilsMessengerEXT";
        error.InstanceProcAddrError.result = res;
        return error;
    }

    return vulkan;
}

Vulkan::Vulkan(
    VkInstancePtr instance,
    VkDevicePtr device,
    VkQueue workQueue,
    VkSurfacePtr surface,
    VkSwapchainPtr swapChain)
    : instance(std::move(instance))
    , device(std::move(device))
    , workQueue(workQueue)
    , surface(std::move(surface))
    , swapChain(std::move(swapChain))
{
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
}

Vulkan::~Vulkan()
{
    // Moving an instance is allowed
    if(auto destroy = vkGetInstanceProcAddrQ(instance, vkDestroyDebugUtilsMessengerEXT); destroy)
    {
        destroy(instance, msg, nullptr);
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
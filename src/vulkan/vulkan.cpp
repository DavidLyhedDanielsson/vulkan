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
    auto createDebugUtilsMessenger =
        vkGetInstanceProcAddrQ(instance, vkCreateDebugUtilsMessengerEXT);
    if(!createDebugUtilsMessenger)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrNotFound.instanceProc = "vkCreateDebugUtilsMessengerEXT";
        return error;
    }
    VkDebugUtilsMessengerEXT msg;
    auto res = createDebugUtilsMessenger(instance, &msgCreateInfo, nullptr, &msg);
    if(res != VK_SUCCESS)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrError.instanceProc = "vkCreateDebugUtilsMessengerEXT";
        error.InstanceProcAddrError.result = res;
        return error;
    }

    VkSurfaceKHR surfaceRaw;
    res = glfwCreateWindowSurface(instance, windowHandle, nullptr, &surfaceRaw);
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

    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(deviceInfo.device, deviceInfo.swapChain, &imageCount, nullptr);
    std::vector<VkImage> swapChainImages(imageCount);
    vkGetSwapchainImagesKHR(
        deviceInfo.device,
        deviceInfo.swapChain,
        &imageCount,
        swapChainImages.data());

    std::vector<VkImageViewPtr> swapChainImageViews;
    swapChainImageViews.reserve(imageCount);
    for(VkImage image : swapChainImages)
    {
        VkImageViewCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB, // TODO
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkImageView imageViewRaw;
        auto res = vkCreateImageView(deviceInfo.device, &imageCreateInfo, nullptr, &imageViewRaw);
        if(res != VK_SUCCESS)
        {
            error.type = ErrorType::OutOfMemory;
            error.OutOfMemory.result = res;
            error.OutOfMemory.message = "vkCreateImageView - swapchain";
            return error;
        }

        swapChainImageViews.emplace_back(VkImageViewPtr(deviceInfo.device, imageViewRaw));
    }

    Vulkan vulkan(
        std::move(instance),
        msg,
        std::move(deviceInfo.device),
        workQueue,
        std::move(surface),
        std::move(deviceInfo.swapChain),
        std::move(swapChainImages),
        std::move(swapChainImageViews));

    return vulkan;
}

Vulkan::Vulkan(
    VkInstancePtr instance,
    VkDebugUtilsMessengerEXT msg,
    VkDevicePtr device,
    VkQueue workQueue,
    VkSurfacePtr surface,
    VkSwapchainPtr swapChain,
    std::vector<VkImage> swapChainImages,
    std::vector<VkImageViewPtr> swapChainImageViews)
    : instance(std::move(instance))
    , msg(msg)
    , device(std::move(device))
    , workQueue(workQueue)
    , surface(std::move(surface))
    , swapChain(std::move(swapChain))
    , swapChainImages(std::move(swapChainImages))
    , swapChainImageViews(std::move(swapChainImageViews))
{
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
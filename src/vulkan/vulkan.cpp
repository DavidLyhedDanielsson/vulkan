#include <GLFW/glfw3.h>

#include "vulkan.h"
#include <array>
#include <vector>

#include "../stl_utils.h"

#include "device_builder.h"
#include "instance_builder.h"

#define vkGetInstanceProcAddrQ(instance, func) (PFN_##func) instance->getProcAddr(#func)

// https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/samples/CreateDebugUtilsMessenger/CreateDebugUtilsMessenger.cpp
PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const* pAllocator)
{
    return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

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
    auto instance = std::get<vk::UniqueInstance>(std::move(instanceVar));

    vk::DebugUtilsMessengerCreateInfoEXT msgCreateInfo = {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                           | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                           | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
        .pfnUserCallback = Vulkan::debugCallback,
        .pUserData = nullptr,
    };

    pfnVkCreateDebugUtilsMessengerEXT =
        vkGetInstanceProcAddrQ(instance, vkCreateDebugUtilsMessengerEXT);
    if(!pfnVkCreateDebugUtilsMessengerEXT)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrNotFound.instanceProc = "vkCreateDebugUtilsMessengerEXT";
        return error;
    }

    pfnVkDestroyDebugUtilsMessengerEXT =
        vkGetInstanceProcAddrQ(instance, vkDestroyDebugUtilsMessengerEXT);
    if(!pfnVkCreateDebugUtilsMessengerEXT)
    {
        error.type = ErrorType::InstanceProcAddrNotFound;
        error.InstanceProcAddrNotFound.instanceProc = "vkDestroyDebugUtilsMessengerEXT";
        return error;
    }

    auto [cdrcRes, msg] = instance->createDebugUtilsMessengerEXTUnique(msgCreateInfo);

    if(cdrcRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::OutOfMemory;
        error.OutOfMemory.result = cdrcRes;
        error.OutOfMemory.message = "createDebugUtilsMessengerEXTUnique";
        return error;
    }

    VkSurfaceKHR surfaceRaw;
    auto glfwRes = glfwCreateWindowSurface(instance.get(), windowHandle, nullptr, &surfaceRaw);
    if(glfwRes != VK_SUCCESS)
    {
        error.type = ErrorType::Unsupported;
        error.Unsupported.message = "Can't create surface";
        return error;
    }
    vk::UniqueSurfaceKHR surface(surfaceRaw, instance.get());

    // Defaults are fine for now
    auto deviceVar = DeviceBuilder(instance, surface).build();

    if(std::holds_alternative<DeviceBuilder::Error>(deviceVar))
    {
        // TODO: Log error; there is no possibility of user error here (yet)
        error.type = ErrorType::HardwareError;
        return error;
    }
    auto deviceInfo = std::get<DeviceBuilder::Data>(std::move(deviceVar));

    vk::Queue workQueue = deviceInfo.device->getQueue(deviceInfo.queueFamilyProperties.index, 0);
    auto [gsiRes, swapChainImages] =
        deviceInfo.device->getSwapchainImagesKHR(deviceInfo.swapChain.get());
    if(gsiRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::OutOfMemory;
        error.OutOfMemory.result = gsiRes;
        error.OutOfMemory.message = "getSwapchainImagesKHR";
        return error;
    }

    std::vector<vk::UniqueImageView> swapChainImageViews;
    swapChainImageViews.reserve(swapChainImages.size());
    for(vk::Image image : swapChainImages)
    {
        vk::ImageViewCreateInfo imageCreateInfo = {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eB8G8R8A8Srgb, // TODO
            .components =
                {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity,
                },
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        auto [civRes, imageViewRaw] = deviceInfo.device->createImageViewUnique(imageCreateInfo);
        if(civRes != vk::Result::eSuccess)
        {
            error.type = ErrorType::OutOfMemory;
            error.OutOfMemory.result = civRes;
            error.OutOfMemory.message = "vkCreateImageView - swapchain";
            return error;
        }

        swapChainImageViews.push_back(std::move(imageViewRaw));
    }

    Vulkan vulkan(
        std::move(instance),
        std::move(msg),
        std::move(deviceInfo.device),
        workQueue,
        std::move(surface),
        std::move(deviceInfo.swapChain),
        std::move(swapChainImages),
        std::move(swapChainImageViews));

    return vulkan;
}

Vulkan::Vulkan(
    vk::UniqueInstance instance,
    vk::UniqueDebugUtilsMessengerEXT msg,
    vk::UniqueDevice device,
    vk::Queue workQueue,
    vk::UniqueSurfaceKHR surface,
    vk::UniqueSwapchainKHR swapChain,
    std::vector<vk::Image> swapChainImages,
    std::vector<vk::UniqueImageView> swapChainImageViews)
    : instance(std::move(instance))
    , msg(std::move(msg))
    , device(std::move(device))
    , workQueue(workQueue)
    , surface(std::move(surface))
    , swapChain(std::move(swapChain))
    , swapChainImages(std::move(swapChainImages))
    , swapChainImageViews(std::move(swapChainImageViews))
{
}

Vulkan::~Vulkan() {}

VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    return VK_FALSE;
}
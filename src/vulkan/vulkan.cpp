#include <GLFW/glfw3.h>

#include "vulkan.h"
#include <array>
#include <vector>

#include "../stl_utils.h"

#include "../config.h"
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

using Builder = Vulkan::Builder;

Builder::Builder(const Config& config, Window& window): config(config), window(window) {}

Builder& Builder::selectSurfaceFormat(const Builder::SurfaceFormatSelector& selector)
{
    this->surfaceFormatSelector = selector;
    return *this;
}

std::variant<Vulkan, Vulkan::Error> Builder::build()
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
    auto glfwRes =
        glfwCreateWindowSurface(instance.get(), window.getWindowHandle(), nullptr, &surfaceRaw);
    if(glfwRes != VK_SUCCESS)
    {
        error.type = ErrorType::Unsupported;
        error.Unsupported.message = "Can't create surface";
        return error;
    }
    vk::UniqueSurfaceKHR surface(surfaceRaw, instance.get());

    auto deviceBuilder =
        DeviceBuilder(instance, surface)
            .selectGpuWithRenderSupport(
                // Love the config = config syntax
                [config = config](
                    std::optional<PhysicalDeviceInfo>,
                    const PhysicalDeviceInfo& potential) -> std::variant<bool, vk::Result> {
                    return potential.surfaceCapabilities.maxImageCount >= config.backbufferCount;
                });
    //            .selectSurfaceFormat(
    //                [config = config](const PhysicalDeviceInfo& info) ->
    //                std::optional<vk::SurfaceFormatKHR> {
    //                  auto iter = std::find_if(
    //                      entire_collection(info.surfaceFormats),
    //                      [config](vk::SurfaceFormatKHR format) {
    //                        return format.format == config.backbufferFormat;
    //                      });
    //
    //                  if(iter == info.surfaceFormats.end())
    //                      return std::nullopt;
    //                  else
    //                      return *iter;
    //                })

    auto deviceVar = deviceBuilder.build();

    if(std::holds_alternative<DeviceBuilder::Error>(deviceVar))
    {
        // TODO: Log error; there is no possibility of user error here (yet)
        error.type = ErrorType::HardwareError;
        return error;
    }
    auto deviceInfo = std::get<DeviceBuilder::Data>(std::move(deviceVar));

    auto defaultSurfaceFormatSelector =
        [](const PhysicalDeviceInfo& info) -> std::optional<vk::SurfaceFormatKHR> {
        auto iter =
            std::find_if(entire_collection(info.surfaceFormats), [](vk::SurfaceFormatKHR format) {
                return format.format == vk::Format::eB8G8R8A8Srgb;
            });

        if(iter == info.surfaceFormats.end())
            return std::nullopt;
        else
            return *iter;
    };
    std::optional<vk::SurfaceFormatKHR> surfaceFormatOpt =
        (this->surfaceFormatSelector.has_value()
             ? this->surfaceFormatSelector.value()
             : defaultSurfaceFormatSelector)(deviceInfo.physicalDeviceInfo);
    if(!surfaceFormatOpt.has_value())
    {
        error.type = ErrorType::NoSurfaceFormatFound;
        return error;
    }
    vk::SurfaceFormatKHR surfaceFormat = surfaceFormatOpt.value();

    vk::Queue workQueue = deviceInfo.device->getQueue(deviceInfo.queueFamilyProperties.index, 0);

    Vulkan vulkan(
        std::move(instance),
        std::move(msg),
        std::move(deviceInfo.device),
        std::move(workQueue),
        std::move(surface),
        std::move(surfaceFormat),
        std::move(deviceInfo.physicalDeviceInfo),
        std::move(deviceInfo.queueFamilyProperties));

    return vulkan;
}

Vulkan::Vulkan(
    vk::UniqueInstance instance,
    vk::UniqueDebugUtilsMessengerEXT msg,
    vk::UniqueDevice device,
    vk::Queue workQueue,
    vk::UniqueSurfaceKHR surface,
    vk::SurfaceFormatKHR surfaceFormat,
    PhysicalDeviceInfo physicalDeviceInfo,
    QueueFamilyInfo queueFamilyProperties)
    : instance(std::move(instance))
    , msg(std::move(msg))
    , device(std::move(device))
    , workQueue(workQueue)
    , surface(std::move(surface))
    , surfaceFormat(std::move(surfaceFormat))
    , physicalDeviceInfo(std::move(physicalDeviceInfo))
    , queueFamilyProperties(std::move(queueFamilyProperties))
{
}

Vulkan::~Vulkan() {}

VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        return VK_FALSE;

    return VK_FALSE;
}
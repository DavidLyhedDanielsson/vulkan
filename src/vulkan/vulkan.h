#pragma once

#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "device_builder.h"

struct GLFWwindow;

class Vulkan
{
  public:
    enum class ErrorType
    {
        HardwareError,
        ConfigError,
        Unsupported,
        InstanceProcAddrNotFound,
        InstanceProcAddrError,
        OutOfMemory,
    };

    struct Error
    {
        ErrorType type;
        union
        {
            struct
            {
                const char* message;
            } Unsupported;
            struct
            {
                const char* instanceProc;
            } InstanceProcAddrNotFound;
            struct
            {
                const char* instanceProc;
                vk::Result result;
            } InstanceProcAddrError;
            struct
            {
                const char* message;
                vk::Result result;
            } OutOfMemory;
        };
    };

    using CreateType = std::variant<Vulkan, Error>;
    static CreateType createVulkan(GLFWwindow* windowHandle, vk::Format backbufferFormat);

    ~Vulkan();
    Vulkan(Vulkan&&) = default;

  private:
    // Placed here for reference
    constexpr static vk::ApplicationInfo APP_INFO = {
        .pApplicationName = "Vulkan test",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName = "??",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // Constructed through Vulkan::createVulkan
    // The parameter list might be getting a bit long...
    Vulkan(
        vk::UniqueInstance instance,
        vk::UniqueDebugUtilsMessengerEXT msg,
        vk::UniqueDevice device,
        vk::Queue workQueue,
        vk::UniqueSurfaceKHR surface,
        vk::UniqueSwapchainKHR swapChain,
        std::vector<vk::Image> swapChainImages,
        std::vector<vk::UniqueImageView> swapChainImageViews,
        PhysicalDeviceInfo physicalDeviceInfo,
        QueueFamilyInfo queueFamilyProperties);

  public:
    // Don't touch the order or deinitialization will not work as intended
    vk::UniqueInstance instance;
    vk::UniqueDebugUtilsMessengerEXT msg;
    vk::UniqueDevice device;
    vk::Queue workQueue;
    vk::UniqueSurfaceKHR surface;
    vk::UniqueSwapchainKHR swapChain;

    std::vector<vk::Image> swapChainImages;
    std::vector<vk::UniqueImageView> swapChainImageViews;

    PhysicalDeviceInfo physicalDeviceInfo;
    QueueFamilyInfo queueFamilyProperties;
};
#pragma once

#include <variant>
#include <vulkan/vulkan.hpp>

#include "smart_pointers.h"

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
                VkResult result;
            } InstanceProcAddrError;
        };
    };

    using CreateType = std::variant<Vulkan, Error>;
    static CreateType createVulkan(GLFWwindow* windowHandle);

    ~Vulkan();
    Vulkan(Vulkan&&) = default;

  private:
    // Placed here for reference
    constexpr static VkApplicationInfo APP_INFO = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
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
    Vulkan(
        VkInstancePtr instance,
        VkDevicePtr device,
        VkQueue workQueue,
        VkSurfacePtr surface,
        VkSwapchainPtr swapChain);

    // Don't touch the order or deinitialization will not work as intended
    VkInstancePtr instance;
    VkDevicePtr device;
    VkQueue workQueue;
    VkSurfacePtr surface;
    VkSwapchainPtr swapChain;

    std::vector<VkImage> swapChainImages;

    VkDebugUtilsMessengerEXT msg;
};
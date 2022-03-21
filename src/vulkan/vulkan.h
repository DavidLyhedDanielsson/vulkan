#pragma once

#include <variant>
#include <vulkan/vulkan.hpp>

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
        VkInstance instance,
        VkDevice device,
        VkQueue workQueue,
        VkSurfaceKHR surface,
        VkSwapchainKHR swapChain);

    // Don't touch the order or deinitialization will not work as intended
    std::unique_ptr<VkInstance_T, void (*)(VkInstance_T*)> instance;
    std::unique_ptr<VkDevice_T, void (*)(VkDevice_T*)> device;
    VkQueue workQueue;
    // Destroying a surface requires the instance, so the lambda needs to capture `this` or
    // `instance`; requiring std::function
    std::unique_ptr<VkSurfaceKHR_T, std::function<void(VkSurfaceKHR_T*)>> surface;
    std::unique_ptr<VkSwapchainKHR_T, std::function<void(VkSwapchainKHR_T*)>> swapChain;

    std::vector<VkImage> swapChainImages;

    VkDebugUtilsMessengerEXT msg;
};
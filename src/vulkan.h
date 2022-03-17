#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

class Vulkan
{
    friend std::optional<Vulkan> createVulkan();

  public:
    ~Vulkan();
    Vulkan(Vulkan&&) = default;

    static std::optional<Vulkan> createVulkan();

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
    Vulkan(VkInstance instance);

    std::unique_ptr<VkInstance_T, void (*)(VkInstance_T*)> instance;
    VkDebugUtilsMessengerEXT msg;
};
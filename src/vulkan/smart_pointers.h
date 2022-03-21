#pragma once

#include <vulkan/vulkan.hpp>

// Transparent unique_ptr-like wrappers around vulkan types that require deletion
// Using std::unique_ptr requires explicit creation, passing a deleter, and
// using std::unique_ptr::get() over and over
// clang-format off
class VkInstancePtr
{
  public:
    VkInstancePtr(VkInstance instance): instance(instance) {}
    VkInstancePtr(VkInstancePtr&&) = default;
    VkInstancePtr& operator=(VkInstancePtr&&) = default;
    VkInstancePtr(const VkInstancePtr&) = delete;
    VkInstancePtr& operator=(const VkInstancePtr&) = delete;
    ~VkInstancePtr() { vkDestroyInstance(instance, nullptr); }
    operator VkInstance() const { return instance; }

    VkInstance instance;
};

class VkDevicePtr
{
  public:
    VkDevicePtr(VkDevice device): device(device) {}
    VkDevicePtr(VkDevicePtr&&) = default;
    VkDevicePtr& operator=(VkDevicePtr&&) = default;
    VkDevicePtr(const VkDevicePtr&) = delete;
    VkDevicePtr& operator=(const VkDevicePtr&) = delete;
    ~VkDevicePtr() { vkDestroyDevice(device, nullptr); }
    operator VkDevice() const { return device; }

    VkDevice device;
};

class VkSurfacePtr
{
  public:
    VkSurfacePtr(VkInstance instance, VkSurfaceKHR surface): instance(instance), surface(surface) {}
    VkSurfacePtr(VkSurfacePtr&&) = default;
    VkSurfacePtr& operator=(VkSurfacePtr&&) = default;
    VkSurfacePtr(const VkSurfacePtr&) = delete;
    VkSurfacePtr& operator=(const VkSurfacePtr&) = delete;
    ~VkSurfacePtr() { vkDestroySurfaceKHR(instance, surface, nullptr); }
    operator VkSurfaceKHR() const { return surface; }

    VkInstance instance;
    VkSurfaceKHR surface;
};

class VkSwapchainPtr
{
  public:
    VkSwapchainPtr(VkDevice device, VkSwapchainKHR swapchain): device(device), swapchain(swapchain) {}
    VkSwapchainPtr(VkSwapchainPtr&&) = default;
    VkSwapchainPtr& operator=(VkSwapchainPtr&&) = default;
    VkSwapchainPtr(const VkSwapchainPtr&) = delete;
    VkSwapchainPtr& operator=(const VkSwapchainKHR&) = delete;
    ~VkSwapchainPtr() { vkDestroySwapchainKHR(device, swapchain, nullptr); }
    operator VkSwapchainKHR() const { return swapchain; }

    VkDevice device;
    VkSwapchainKHR swapchain;
};

// clang-format on
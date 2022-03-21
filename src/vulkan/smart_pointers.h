#pragma once

#include <vulkan/vulkan.hpp>

#define SAFE_CALL(x, func)  \
    if(x != VK_NULL_HANDLE) \
    {                       \
        func;               \
    }

// Transparent unique_ptr-like wrappers around vulkan types that require deletion
// Using std::unique_ptr requires explicit creation, passing a deleter, and
// using std::unique_ptr::get() over and over
// clang-format off
class VkInstancePtr
{
  public:
    VkInstancePtr(VkInstance instance): instance(instance) {}
    VkInstancePtr(VkInstancePtr&& other) {
        instance = other.instance;
        other.instance = VK_NULL_HANDLE;
    }
    VkInstancePtr& operator=(VkInstancePtr&& other) {
        instance = other.instance;
        other.instance = VK_NULL_HANDLE;
        return *this;
    };
    VkInstancePtr(const VkInstancePtr&) = delete;
    VkInstancePtr& operator=(const VkInstancePtr&) = delete;
    ~VkInstancePtr() { SAFE_CALL(instance, vkDestroyInstance(instance, nullptr)) }
    operator VkInstance() const { return instance; }

    VkInstance instance;
};

class VkDevicePtr
{
  public:
    VkDevicePtr(VkDevice device): device(device) {}
    VkDevicePtr(VkDevicePtr&& other) {
        device = other.device;
        other.device = VK_NULL_HANDLE;
    }
    VkDevicePtr& operator=(VkDevicePtr&& other) {
        device = other.device;
        other.device = VK_NULL_HANDLE;
        return *this;
    }
    VkDevicePtr(const VkDevicePtr&) = delete;
    VkDevicePtr& operator=(const VkDevicePtr&) = delete;
    ~VkDevicePtr() { SAFE_CALL(device, vkDestroyDevice(device, nullptr)) }
    operator VkDevice() const { return device; }

    VkDevice device;
};

class VkSurfacePtr
{
  public:
    VkSurfacePtr(VkInstance instance, VkSurfaceKHR surface): instance(instance), surface(surface) {}
    VkSurfacePtr(VkSurfacePtr&& other) {
        instance = other.instance;
        surface = other.surface;
        other.surface = VK_NULL_HANDLE;
    }
    VkSurfacePtr& operator=(VkSurfacePtr&& other) {
        instance = other.instance;
        surface = other.surface;
        other.surface = VK_NULL_HANDLE;
        return *this;
    }
    VkSurfacePtr(const VkSurfacePtr&) = delete;
    VkSurfacePtr& operator=(const VkSurfacePtr&) = delete;
    ~VkSurfacePtr() { SAFE_CALL(surface, vkDestroySurfaceKHR(instance, surface, nullptr)) }
    operator VkSurfaceKHR() const { return surface; }

    VkInstance instance;
    VkSurfaceKHR surface;
};

class VkSwapchainPtr
{
  public:
    VkSwapchainPtr(VkDevice device, VkSwapchainKHR swapchain): device(device), swapchain(swapchain) {}
    VkSwapchainPtr(VkSwapchainPtr&& other) {
        device = other.device;
        swapchain = other.swapchain;
        other.swapchain = VK_NULL_HANDLE;
    }
    VkSwapchainPtr& operator=(VkSwapchainPtr&& other) {
        device = other.device;
        swapchain = other.swapchain;
        other.swapchain = VK_NULL_HANDLE;
        return *this;
    }
    VkSwapchainPtr(const VkSwapchainPtr&) = delete;
    VkSwapchainPtr& operator=(const VkSwapchainKHR&) = delete;
    ~VkSwapchainPtr() { SAFE_CALL(swapchain, vkDestroySwapchainKHR(device, swapchain, nullptr)) }
    operator VkSwapchainKHR() const { return swapchain; }

    VkDevice device;
    VkSwapchainKHR swapchain;
};

class VkImageViewPtr
{
  public:
    VkImageViewPtr(VkDevice device, VkImageView imageView): device(device), imageView(imageView) {}
    VkImageViewPtr(VkImageViewPtr&& other) {
        device = other.device;
        imageView = other.imageView;
        other.imageView = VK_NULL_HANDLE;
    }
    VkImageViewPtr& operator=(VkImageViewPtr&& other) {
        device = other.device;
        imageView = other.imageView;
        other.imageView = VK_NULL_HANDLE;
        return *this;
    }
    VkImageViewPtr(const VkImageViewPtr&) = delete;
    VkImageViewPtr& operator=(const VkSwapchainKHR&) = delete;
    ~VkImageViewPtr() { SAFE_CALL(imageView, vkDestroyImageView(device, imageView, nullptr)) }
    operator VkImageView() const { return imageView; }

    VkDevice device;
    VkImageView imageView;
};

// clang-format on
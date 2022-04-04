#pragma once

#include <vulkan/vulkan.hpp>

// Contains data/config that the user sets; things from an options menu
struct UserConfig
{
    uint32_t resolutionWidth;
    uint32_t resolutionHeight;
    vk::Format backbufferFormat;
    vk::SampleCountFlagBits
        sampleCount; // This is a bit unclear and should probably be changed to an enum
    uint32_t backbufferCount;
};

// Contains data/config about things selected/configured at runtime
struct SelectedConfig
{
    // Remember: Order matters due to deinitialization!
    vk::UniqueInstance instance;
    vk::UniqueDevice device;
    vk::PhysicalDevice physicalDevice;

    struct Queues
    {
        struct WorkQueue
        {
            uint32_t index;
            vk::QueueFamilyProperties properties;
            vk::Queue queue;
        } workQueueInfo;
    } queues;

    struct Surface
    {
        vk::UniqueSurfaceKHR surface;
        vk::SurfaceFormatKHR format;
    } surfaceConfig;

    struct SwapChain
    {
        vk::UniqueSwapchainKHR swapchain;
        std::vector<vk::Image> images;
        std::vector<vk::UniqueImageView> imageViews;
        std::vector<vk::UniqueFramebuffer> framebuffers;
    } swapchainConfig;

    struct Debug
    {
        vk::UniqueDebugUtilsMessengerEXT msg;
    } debug;

    struct Pipeline
    {
        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout layout;
        vk::UniqueRenderPass renderPass;
        vk::Rect2D renderArea;
    } pipelineConfig;
};

#pragma once

#include <optional>
#include <variant>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "../config.h"

class Swapchain
{
  public:
    class Builder
    {
        using Self = Builder;

      public:
        enum class ErrorType
        {
            SwapChainCreationError,
            OutOfMemory,
        };

        struct Error
        {
            ErrorType type;
            union
            {
                struct
                {
                    vk::Result result;
                } SwapChainCreationError;
                struct
                {
                    const char* message;
                    vk::Result result;
                } OutOfMemory;
            };
        };

        Builder(
            const Config& config,
            const vk::UniqueSurfaceKHR& surface,
            const vk::UniqueDevice& device);

        Self withBackbufferFormat(vk::Format);
        Self withColorSpace(vk::ColorSpaceKHR);
        Self withPresentMode(vk::PresentModeKHR);
        Self createFramebuffersFor(vk::UniqueRenderPass&);

        using BuildType = std::variant<Swapchain, Error>;
        BuildType build();

      private:
        const Config& config;
        const vk::UniqueSurfaceKHR& surface;
        const vk::UniqueDevice& device;

        std::optional<vk::UniqueRenderPass*> renderPass;
        std::optional<vk::Format> backbufferFormat;
        std::optional<vk::ColorSpaceKHR> backbufferColorSpace;
        std::optional<vk::PresentModeKHR> presentMode;
        std::optional<vk::Extent2D> extent;
    };

    Swapchain(
        vk::UniqueSwapchainKHR swapChain,
        std::vector<vk::Image> swapChainImages,
        std::vector<vk::UniqueImageView> swapChainImageViews,
        std::vector<vk::UniqueFramebuffer> framebuffers);

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = default;
    Swapchain& operator=(Swapchain&&) = default;

    void reset();

    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::UniqueImageView> swapchainImageViews;
    std::vector<vk::UniqueFramebuffer> framebuffers;
};
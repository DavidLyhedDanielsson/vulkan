#pragma once

#include <optional>
#include <variant>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "../config.h"

class SwapchainBuilder
{
    using Self = SwapchainBuilder;

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

    SwapchainBuilder(
        const UserConfig& config,
        const vk::UniqueSurfaceKHR& surface,
        const vk::UniqueDevice& device);

    Self withBackbufferFormat(vk::Format);
    Self withColorSpace(vk::ColorSpaceKHR);
    Self withPresentMode(vk::PresentModeKHR);
    Self createFramebuffersFor(vk::UniqueRenderPass&);

    std::optional<Error> build(SelectedConfig::SwapChain& swapChainData);

  private:
    const UserConfig& config;
    const vk::UniqueSurfaceKHR& surface;
    const vk::UniqueDevice& device;

    std::optional<vk::UniqueRenderPass*> renderPass;
    std::optional<vk::Format> backbufferFormat;
    std::optional<vk::ColorSpaceKHR> backbufferColorSpace;
    std::optional<vk::PresentModeKHR> presentMode;
    std::optional<vk::Extent2D> extent;
};
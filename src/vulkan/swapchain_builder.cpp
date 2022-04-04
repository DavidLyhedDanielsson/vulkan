#include "swapchain_builder.h"

using Self = SwapchainBuilder;

SwapchainBuilder::SwapchainBuilder(
    const UserConfig& config,
    const vk::UniqueSurfaceKHR& surface,
    const vk::UniqueDevice& device)
    : config(config)
    , surface(surface)
    , device(device)
{
}

std::optional<SwapchainBuilder::Error> SwapchainBuilder::build(
    SelectedConfig::SwapChain& swapChainData)
{
    Error error;

    vk::SwapchainCreateInfoKHR swapchainCreateInfo = {
        .surface = surface.get(),
        .minImageCount = config.backbufferCount,
        .imageFormat = this->backbufferFormat.value_or(vk::Format::eB8G8R8A8Snorm),
        .imageColorSpace = this->backbufferColorSpace.value_or(vk::ColorSpaceKHR::eSrgbNonlinear),
        .imageExtent =
            extent.value_or(vk::Extent2D{config.resolutionWidth, config.resolutionHeight}),
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode.value_or(vk::PresentModeKHR::eFifo),
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    auto [csRes, swapchain] = device->createSwapchainKHRUnique(swapchainCreateInfo);
    if(csRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::SwapChainCreationError;
        error.SwapChainCreationError.result = csRes;
        return error;
    }

    auto [gsiRes, images] = device->getSwapchainImagesKHR(swapchain.get());
    if(gsiRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::OutOfMemory;
        error.OutOfMemory.result = gsiRes;
        error.OutOfMemory.message = "getSwapchainImagesKHR";
        return error;
    }

    std::vector<vk::UniqueImageView> imageViews;
    imageViews.reserve(images.size());
    for(vk::Image image : images)
    {
        vk::ImageViewCreateInfo imageCreateInfo = {
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = config.backbufferFormat,
            .components =
                {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity,
                },
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        auto [civRes, imageViewRaw] = device->createImageViewUnique(imageCreateInfo);
        if(civRes != vk::Result::eSuccess)
        {
            error.type = ErrorType::OutOfMemory;
            error.OutOfMemory.result = civRes;
            error.OutOfMemory.message = "vkCreateImageView - swapchain";
            return error;
        }

        imageViews.push_back(std::move(imageViewRaw));
    }

    std::vector<vk::UniqueFramebuffer> framebuffers;
    if(this->renderPass.has_value())
    {
        for(const auto& swapchainImageView : imageViews)
        {
            vk::FramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = this->renderPass.value()->get(),
                .attachmentCount = 1,
                .pAttachments = &swapchainImageView.get(),
                .width = config.resolutionWidth,
                .height = config.resolutionHeight,
                .layers = 1,
            };

            auto [cfRes, framebuffer] = device->createFramebufferUnique(framebufferCreateInfo);
            assert(cfRes == vk::Result::eSuccess);
            framebuffers.push_back(std::move(framebuffer));
        }
    }

    swapChainData.swapchain = std::move(swapchain);
    swapChainData.images = std::move(images);
    swapChainData.imageViews = std::move(imageViews);
    swapChainData.framebuffers = std::move(framebuffers);

    return std::nullopt;
}

Self SwapchainBuilder::withBackbufferFormat(vk::Format format)
{
    this->backbufferFormat = format;
    return *this;
}

Self SwapchainBuilder::withColorSpace(vk::ColorSpaceKHR colorSpace)
{
    this->backbufferColorSpace = colorSpace;
    return *this;
}

Self SwapchainBuilder::withPresentMode(vk::PresentModeKHR presentMode)
{
    this->presentMode = presentMode;
    return *this;
}

Self SwapchainBuilder::createFramebuffersFor(vk::UniqueRenderPass& renderPass)
{
    this->renderPass = &renderPass;
    return *this;
}

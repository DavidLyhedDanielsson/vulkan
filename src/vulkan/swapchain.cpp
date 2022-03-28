#include "swapchain.h"

using Builder = Swapchain::Builder;

Swapchain::Builder::Builder(
    const Config& config,
    const vk::UniqueSurfaceKHR& surface,
    const vk::UniqueDevice& device)
    : config(config)
    , surface(surface)
    , device(device)
{
}

Builder::BuildType Swapchain::Builder::build()
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

    auto [gsiRes, swapchainImages] = device->getSwapchainImagesKHR(swapchain.get());
    if(gsiRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::OutOfMemory;
        error.OutOfMemory.result = gsiRes;
        error.OutOfMemory.message = "getSwapchainImagesKHR";
        return error;
    }

    std::vector<vk::UniqueImageView> swapchainImageViews;
    swapchainImageViews.reserve(swapchainImages.size());
    for(vk::Image image : swapchainImages)
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

        swapchainImageViews.push_back(std::move(imageViewRaw));
    }

    std::vector<vk::UniqueFramebuffer> framebuffers;
    if(this->renderPass.has_value())
    {
        for(const auto& swapchainImageView : swapchainImageViews)
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

    Swapchain s(
        std::move(swapchain),
        std::move(swapchainImages),
        std::move(swapchainImageViews),
        std::move(framebuffers));

    return std::variant<Swapchain, Error>(std::move(s));
}

Builder::Self Swapchain::Builder::withBackbufferFormat(vk::Format format)
{
    this->backbufferFormat = format;
    return *this;
}

Builder::Self Swapchain::Builder::withColorSpace(vk::ColorSpaceKHR colorSpace)
{
    this->backbufferColorSpace = colorSpace;
    return *this;
}

Builder::Self Swapchain::Builder::withPresentMode(vk::PresentModeKHR presentMode)
{
    this->presentMode = presentMode;
    return *this;
}

Builder::Self Swapchain::Builder::createFramebuffersFor(vk::UniqueRenderPass& renderPass)
{
    this->renderPass = &renderPass;
    return *this;
}

Swapchain::Swapchain(
    vk::UniqueSwapchainKHR swapChain,
    std::vector<vk::Image> swapChainImages,
    std::vector<vk::UniqueImageView> swapchainImageViews,
    std::vector<vk::UniqueFramebuffer> framebuffers)
    : swapchain(std::move(swapChain))
    , swapchainImages(std::move(swapChainImages))
    , swapchainImageViews(std::move(swapchainImageViews))
    , framebuffers(std::move(framebuffers))
{
}

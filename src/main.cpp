#include <cassert>
#include <iostream>

#include <vulkan/vulkan.h>

#include "config.h"
#include "shader_registry.h"
//#include "vulkan/pipeline.h"
#include "vulkan/pipeline.h"
#include "vulkan/vulkan.h"
#include "window.h"

#include "shader_paths.h"

int main()
{
    glfwInit();

    Config config = {
        .resolutionWidth = 1280,
        .resolutionHeight = 720,
        .backbufferFormat = vk::Format::eB8G8R8A8Srgb,
        .sampleCount = vk::SampleCountFlagBits::e1,
    };

    // Create window
    auto mainWindowOpt = Window::createWindow(
        (int)config.resolutionWidth,
        (int)config.resolutionHeight,
        "Vulkan window");
    assert(mainWindowOpt.has_value());
    Window mainWindow = std::move(mainWindowOpt.value());

    // Create vulkan instances
    auto vulkanVar = Vulkan::createVulkan(mainWindow.getWindowHandle(), config.backbufferFormat);
    assert(!std::holds_alternative<Vulkan::Error>(vulkanVar));
    auto vulkan = std::get<Vulkan>(std::move(vulkanVar));

    // Preload shaders
    ShaderRegistry shaderRegistry;
    assert(!shaderRegistry.loadVertexShader(vulkan.device, ShaderPaths::FakeVertex).has_value());
    assert(
        !shaderRegistry.loadFragmentShader(vulkan.device, ShaderPaths::FakeFragment).has_value());

    Pipeline pipeline =
        Pipeline::Builder()
            .usingConfig(config)
            .usingShaderRegistry(shaderRegistry)
            .usingDevice(vulkan.device)
            .withVertexShader(ShaderPaths::FakeVertex)
            .withFragmentShader(ShaderPaths::FakeFragment)
            .withPrimitiveTopology(Pipeline::Builder::PrimitiveTopology::TriangleList)
            .withViewport(Pipeline::Builder::Viewport::Fullscreen)
            .withRasterizerState(Pipeline::Builder::Rasterizer::BackfaceCulling)
            .withMultisampleState(Pipeline::Builder::Multisample::Disabled)
            .withBlendState(Pipeline::Builder::Blend::Disabled)
            .build();

    std::vector<vk::UniqueFramebuffer> framebuffers;
    for(const auto& swapchainImageView : vulkan.swapChainImageViews)
    {
        vk::FramebufferCreateInfo framebufferCreateInfo = {
            .renderPass = pipeline.renderPass.get(),
            .attachmentCount = 1,
            .pAttachments = &swapchainImageView.get(),
            .width = config.resolutionWidth,
            .height = config.resolutionHeight,
            .layers = 1,
        };

        auto [cfRes, framebuffer] = vulkan.device->createFramebufferUnique(framebufferCreateInfo);
        assert(cfRes == vk::Result::eSuccess);
        framebuffers.push_back(std::move(framebuffer));
    }

    auto [ccpRes, commandPool] = vulkan.device->createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = vulkan.queueFamilyProperties.index,
    });
    assert(ccpRes == vk::Result::eSuccess);

    auto [acbRes, commandBuffers] = vulkan.device->allocateCommandBuffersUnique({
        .commandPool = commandPool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    assert(acbRes == vk::Result::eSuccess);

    auto commandBuffer = std::move(commandBuffers[0]);

    vk::ClearValue clearValue = {std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f})};

    auto [iasRes, imageAvailable] = vulkan.device->createSemaphoreUnique({});
    assert(iasRes == vk::Result::eSuccess);
    auto [rfsRes, renderFinished] = vulkan.device->createSemaphoreUnique({});
    assert(rfsRes == vk::Result::eSuccess);
    auto [fRes, fence] =
        vulkan.device->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled});
    assert(fRes == vk::Result::eSuccess);

    // uint32_t swapChainImage = 0;
    while(!mainWindow.shouldClose())
    {
        mainWindow.pollEvents();

        auto wffRes = vulkan.device->waitForFences(fence.get(), true, UINT64_MAX);
        assert(wffRes == vk::Result::eSuccess);
        vulkan.device->resetFences(fence.get());

        auto [acnRes, swapChainImageIndex] = vulkan.device->acquireNextImageKHR(
            vulkan.swapChain.get(),
            UINT64_MAX,
            imageAvailable.get(),
            VK_NULL_HANDLE);

        commandBuffer->reset();
        vk::CommandBufferBeginInfo beginInfo = {};
        assert(commandBuffer->begin(beginInfo) == vk::Result::eSuccess);
        commandBuffer->beginRenderPass(
            {
                .renderPass = pipeline.renderPass.get(),
                .framebuffer = framebuffers[swapChainImageIndex].get(),
                .renderArea = pipeline.renderArea,
                .clearValueCount = 1,
                .pClearValues = &clearValue,
            },
            vk::SubpassContents::eInline);
        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline.get());
        commandBuffer->draw(3, 1, 0, 0);
        commandBuffer->endRenderPass();
        assert(commandBuffer->end() == vk::Result::eSuccess);

        vk::PipelineStageFlags waitDstStageMask[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
        };
        assert(
            vulkan.workQueue.submit(
                {{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &imageAvailable.get(),
                    .pWaitDstStageMask = waitDstStageMask,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer.get(),
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &renderFinished.get(),
                }},
                fence.get())
            == vk::Result::eSuccess);

        vk::PresentInfoKHR presentInfo = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinished.get(),
            .swapchainCount = 1,
            .pSwapchains = &vulkan.swapChain.get(),
            .pImageIndices = &swapChainImageIndex,
            .pResults = nullptr,
        };

        assert(vulkan.workQueue.presentKHR(presentInfo) == vk::Result::eSuccess);
    }

    assert(vulkan.device->waitIdle() == vk::Result::eSuccess);

    glfwTerminate();
    return 0;
}
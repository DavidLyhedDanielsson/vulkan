#include <cassert>
#include <iostream>

#include <vulkan/vulkan.h>

#include "config.h"
#include "shader_registry.h"
#include "vulkan/pipeline.h"
#include "vulkan/swapchain.h"
#include "vulkan/vulkan.h"
#include "window.h"

#include "shader_paths.h"

struct WindowSize
{
    uint32_t width;
    uint32_t height;
};

int main()
{
    glfwInit();

    Config config = {
        .resolutionWidth = 1280,
        .resolutionHeight = 720,
        .backbufferFormat = vk::Format::eB8G8R8A8Srgb,
        .sampleCount = vk::SampleCountFlagBits::e1,
        .backbufferCount = 3,
    };

    //  Create window
    std::optional<WindowSize> windowResized;
    auto mainWindowOpt = Window::createWindow(
        (int)config.resolutionWidth,
        (int)config.resolutionHeight,
        "Vulkan window",
        [&windowResized](uint32_t width, uint32_t height) {
            windowResized = {width, height};
        });
    assert(mainWindowOpt.has_value());
    std::unique_ptr<Window> mainWindow = std::move(mainWindowOpt.value());

    // Create vulkan instances
    auto vulkanVar = Vulkan::Builder(config, *mainWindow).build();
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

    auto swapchainVar = Swapchain::Builder(config, vulkan.surface, vulkan.device)
                            .withBackbufferFormat(vulkan.surfaceFormat.format)
                            .withColorSpace(vulkan.surfaceFormat.colorSpace)
                            .createFramebuffersFor(pipeline.renderPass)
                            .build();
    assert(!std::holds_alternative<Swapchain::Builder::Error>(swapchainVar));
    auto swapchain = std::get<Swapchain>(std::move(swapchainVar));
    // Command buffers

    auto [ccpRes, commandPool] = vulkan.device->createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = vulkan.queueFamilyProperties.index,
    });
    assert(ccpRes == vk::Result::eSuccess);

    auto [acbRes, commandBuffers] = vulkan.device->allocateCommandBuffersUnique({
        .commandPool = commandPool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 3,
    });
    assert(acbRes == vk::Result::eSuccess);

    // Sync
    std::vector<vk::UniqueSemaphore> imageAvailableList(config.backbufferCount);
    std::vector<vk::UniqueSemaphore> renderFinishedList(config.backbufferCount);
    std::vector<vk::UniqueFence> fences(config.backbufferCount);
    for(uint32_t i = 0; i < config.backbufferCount; ++i)
    {
        auto [iasRes, imageAvailable] = vulkan.device->createSemaphoreUnique({});
        assert(iasRes == vk::Result::eSuccess);
        imageAvailableList[i] = std::move(imageAvailable);

        auto [rfsRes, renderFinished] = vulkan.device->createSemaphoreUnique({});
        assert(rfsRes == vk::Result::eSuccess);
        renderFinishedList[i] = std::move(renderFinished);

        auto [fRes, fence] = vulkan.device->createFenceUnique({
            .flags = vk::FenceCreateFlagBits::eSignaled,
        });
        assert(fRes == vk::Result::eSuccess);
        fences[i] = std::move(fence);
    }

    uint32_t frame = 0;
    uint32_t backbufferFrame = 0;
    while(!mainWindow->shouldClose())
    {
        mainWindow->pollEvents();

        if(windowResized.has_value())
        {
            auto [newWidth, newHeight] = windowResized.value();

            assert(vulkan.device->waitIdle() == vk::Result::eSuccess);

            pipeline.release();
        }

        auto wffRes = vulkan.device->waitForFences(fences[backbufferFrame].get(), true, UINT64_MAX);
        assert(wffRes == vk::Result::eSuccess);
        vulkan.device->resetFences(fences[backbufferFrame].get());

        auto [acnRes, swapchainImageIndex] = vulkan.device->acquireNextImageKHR(
            swapchain.swapchain.get(),
            UINT64_MAX,
            imageAvailableList[backbufferFrame].get(),
            VK_NULL_HANDLE);

        auto& commandBuffer = commandBuffers[backbufferFrame];
        commandBuffer->reset();
        vk::CommandBufferBeginInfo beginInfo = {};
        assert(commandBuffer->begin(beginInfo) == vk::Result::eSuccess);

        vk::ClearValue clearValue = {std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f})};
        commandBuffer->beginRenderPass(
            {
                .renderPass = pipeline.renderPass.get(),
                .framebuffer = swapchain.framebuffers[swapchainImageIndex].get(),
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
                    .pWaitSemaphores = &imageAvailableList[backbufferFrame].get(),
                    .pWaitDstStageMask = waitDstStageMask,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer.get(),
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = &renderFinishedList[backbufferFrame].get(),
                }},
                fences[backbufferFrame].get())
            == vk::Result::eSuccess);

        vk::PresentInfoKHR presentInfo = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinishedList[backbufferFrame].get(),
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain.get(),
            .pImageIndices = &swapchainImageIndex,
            .pResults = nullptr,
        };

        assert(vulkan.workQueue.presentKHR(presentInfo) == vk::Result::eSuccess);

        frame++;
        backbufferFrame = frame % config.backbufferCount;
    }

    assert(vulkan.device->waitIdle() == vk::Result::eSuccess);

    glfwTerminate();
    return 0;
}
#include <cassert>
#include <iostream>

#include <vulkan/vulkan.h>

#include "config.h"
#include "shader_registry.h"
#include "vertex.h"
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

    auto pipelineBuilder =
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
            .withLinearVertexLayout<TriangleVertex>(
                vk::Format::eR32G32Sfloat,
                vk::Format::eR32G32B32Sfloat);

    Pipeline pipeline = pipelineBuilder.build();

    auto swapchainBuilder = Swapchain::Builder(config, vulkan.surface, vulkan.device)
                                .withBackbufferFormat(vulkan.surfaceFormat.format)
                                .withColorSpace(vulkan.surfaceFormat.colorSpace)
                                .createFramebuffersFor(pipeline.renderPass);
    auto swapchainVar = swapchainBuilder.build();
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

    std::vector<TriangleVertex> vertices = {
        TriangleVertex{{-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        TriangleVertex{{0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        TriangleVertex{{0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},

        TriangleVertex{{-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        TriangleVertex{{0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        TriangleVertex{{0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    };
    vk::BufferCreateInfo bufferInfo = {
        .size = sizeof(TriangleVertex) * vertices.size(),
        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    auto [res, vertexBuffer] = vulkan.device->createBufferUnique(bufferInfo);
    assert(res == vk::Result::eSuccess);

    auto memoryProperties = vulkan.physicalDeviceInfo.device.getMemoryProperties();
    auto memoryRequirements = vulkan.device->getBufferMemoryRequirements(vertexBuffer.get());

    std::optional<uint32_t> memoryIndexOpt = 0;
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const auto required =
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        if(memoryRequirements.memoryTypeBits & (1 << i)
           && (memoryProperties.memoryTypes[i].propertyFlags & required))
        {
            memoryIndexOpt = i;
            break;
        }
    }
    assert(memoryIndexOpt.has_value());
    uint32_t memoryIndex = memoryIndexOpt.value();

    vk::MemoryAllocateInfo allocateInfo = {
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryIndex,
    };
    auto [amRes, memory] = vulkan.device->allocateMemoryUnique(allocateInfo);
    assert(amRes == vk::Result::eSuccess);
    assert(
        vulkan.device->bindBufferMemory(vertexBuffer.get(), memory.get(), 0)
        == vk::Result::eSuccess);

    void* data;
    assert(
        vulkan.device->mapMemory(memory.get(), 0, bufferInfo.size, vk::MemoryMapFlags(), &data)
        == vk::Result::eSuccess);
    std::memcpy(data, vertices.data(), bufferInfo.size);
    vulkan.device->unmapMemory(memory.get());

    bool recreateSwapchain = false;
    uint32_t frame = 0;
    uint32_t backbufferFrame = 0;
    while(!mainWindow->shouldClose())
    {
        mainWindow->pollEvents();

        if(windowResized.has_value() || recreateSwapchain)
        {
            if(windowResized.has_value())
            {
                auto [newWidth, newHeight] = windowResized.value();
                windowResized.reset();
                config.resolutionWidth = newWidth;
                config.resolutionHeight = newHeight;
                std::cout << "Resizing to " << newWidth << "; " << newHeight << std::endl;
            }
            else
            {
                std::cout << "Resizing without info";
            }

            // I can't verify this because i3wm never minimizes, but vulkan-tutorial says this works
            // :)
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(mainWindow->getWindowHandle(), &width, &height);
            bool minimized = false;
            while(width == 0 || height == 0)
            {
                minimized = true;
                glfwGetFramebufferSize(mainWindow->getWindowHandle(), &width, &height);
                glfwWaitEvents();
            }
            if(minimized)
                continue;

            assert(vulkan.device->waitIdle() == vk::Result::eSuccess);

            swapchain.reset();
            pipeline.reset();

            pipeline = pipelineBuilder.build();
            swapchainBuilder.createFramebuffersFor(pipeline.renderPass);
            auto swapchainVar = swapchainBuilder.build();
            assert(std::holds_alternative<Swapchain>(swapchainVar));
            swapchain = std::move(std::get<Swapchain>(swapchainVar));
            recreateSwapchain = false;

            // The swapchain might already be invalid if the window is still being resized
            continue;
        }

        auto wffRes = vulkan.device->waitForFences(fences[backbufferFrame].get(), true, UINT64_MAX);
        assert(wffRes == vk::Result::eSuccess);

#define handleRetError(Fun)                                                            \
    {                                                                                  \
        auto res = Fun;                                                                \
        if(res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR) \
        {                                                                              \
            recreateSwapchain = true;                                                  \
            continue;                                                                  \
        }                                                                              \
        else                                                                           \
        {                                                                              \
            assert(res == vk::Result::eSuccess);                                       \
        }                                                                              \
    }

#define handleError(val)                                                                   \
    {                                                                                      \
        if((val) == vk::Result::eErrorOutOfDateKHR || (val) == vk::Result::eSuboptimalKHR) \
        {                                                                                  \
            recreateSwapchain = true;                                                      \
            continue;                                                                      \
        }                                                                                  \
        else                                                                               \
        {                                                                                  \
            assert((val) == vk::Result::eSuccess);                                         \
        }                                                                                  \
    }

        auto [acnRes, swapchainImageIndex] = vulkan.device->acquireNextImageKHR(
            swapchain.swapchain.get(),
            UINT64_MAX,
            imageAvailableList[backbufferFrame].get(),
            VK_NULL_HANDLE);
        handleError(acnRes);

        vulkan.device->resetFences(fences[backbufferFrame].get());

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
        vk::DeviceSize offset = 0;
        commandBuffer->bindVertexBuffers(0, 1, &vertexBuffer.get(), &offset);
        commandBuffer->draw(vertices.size(), 1, 0, 0);
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

        handleRetError(vulkan.workQueue.presentKHR(presentInfo));

        frame++;
        backbufferFrame = frame % config.backbufferCount;
    }

    assert(vulkan.device->waitIdle() == vk::Result::eSuccess);

    glfwTerminate();
    return 0;
}
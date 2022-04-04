#include <cassert>
#include <iostream>
#include <variant>

#include <vulkan/vulkan.h>

#include "config.h"
#include "shader_registry.h"
#include "stl_utils.h"
#include "vertex.h"
#include "vulkan/device_builder.h"
#include "vulkan/instance_builder.h"
#include "vulkan/pipeline_builder.h"
#include "vulkan/swapchain_builder.h"
#include "window.h"

#include "shader_paths.h"
#include "vulkan/buffer.h"

#define vkGetInstanceProcAddrQ(instance, func) (PFN_##func) instance->getProcAddr(#func)

// https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/samples/CreateDebugUtilsMessenger/CreateDebugUtilsMessenger.cpp
PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const* pAllocator)
{
    return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        return VK_FALSE;

    // vkCreateSwapchainKHR called with invalid imageExtent
    if(pCallbackData->messageIdNumber == 0x7cd0911d)
        return VK_FALSE;

    std::cout << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

template<typename T, typename E>
T expectResult(std::variant<T, E> var)
{
    assert(std::holds_alternative<T>(var));
    return std::move(std::get<T>(var));
}

int main()
{
    glfwInit();

    UserConfig config = {
        .resolutionWidth = 1280,
        .resolutionHeight = 720,
        .backbufferFormat = vk::Format::eB8G8R8A8Srgb,
        .sampleCount = vk::SampleCountFlagBits::e1,
        .backbufferCount = 3,
    };

    SelectedConfig selectedConfig;

    //  Create window
    bool windowResized = false;
    auto mainWindowOpt = Window::createWindow(
        (int)config.resolutionWidth,
        (int)config.resolutionHeight,
        "Vulkan window",
        [&windowResized](uint32_t, uint32_t) { windowResized = true; });
    assert(mainWindowOpt.has_value());
    std::unique_ptr<Window> mainWindow = std::move(mainWindowOpt.value());

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    assert(!InstanceBuilder()
                .withValidationLayer()
                .withDebugExtension()
                .withRequiredExtensions(glfwExtensions, glfwExtensionCount)
                .build(selectedConfig)
                .has_value());

    {
        pfnVkCreateDebugUtilsMessengerEXT =
            vkGetInstanceProcAddrQ(*selectedConfig.instance, vkCreateDebugUtilsMessengerEXT);
        assert(pfnVkCreateDebugUtilsMessengerEXT);

        pfnVkDestroyDebugUtilsMessengerEXT =
            vkGetInstanceProcAddrQ(*selectedConfig.instance, vkDestroyDebugUtilsMessengerEXT);
        assert(pfnVkCreateDebugUtilsMessengerEXT);

        vk::DebugUtilsMessengerCreateInfoEXT msgCreateInfo = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr,
        };

        auto [cdrcRes, msg] =
            selectedConfig.instance->createDebugUtilsMessengerEXTUnique(msgCreateInfo);
        assert(cdrcRes == vk::Result::eSuccess);
        selectedConfig.debug.msg = std::move(msg);
    }

    {
        VkSurfaceKHR surfaceRaw;
        assert(
            glfwCreateWindowSurface(
                *selectedConfig.instance,
                mainWindow->getWindowHandle(),
                nullptr,
                &surfaceRaw)
            == VK_SUCCESS);
        selectedConfig.surfaceConfig.surface =
            vk::UniqueSurfaceKHR(surfaceRaw, *selectedConfig.instance);
    }
    auto dbRes =
        DeviceBuilder(selectedConfig.instance, selectedConfig.surfaceConfig.surface)
            .selectGpuWithRenderSupport(
                [&](std::optional<vk::PhysicalDevice>,
                    const vk::PhysicalDevice& potential) -> std::variant<bool, vk::Result> {
                    auto capabilities =
                        potential.getSurfaceCapabilitiesKHR(*selectedConfig.surfaceConfig.surface)
                            .value;
                    return config.backbufferCount >= capabilities.minImageCount
                           && config.backbufferCount <= capabilities.maxImageCount;
                })
            .build(selectedConfig);
    assert(!dbRes.has_value());
    {
        auto surfaceFormats = selectedConfig.physicalDevice
                                  .getSurfaceFormatsKHR(*selectedConfig.surfaceConfig.surface)
                                  .value;
        auto iter =
            std::find_if(entire_collection(surfaceFormats), [&](vk::SurfaceFormatKHR format) {
                return format.format == config.backbufferFormat;
            });

        assert(iter != surfaceFormats.end());
        selectedConfig.surfaceConfig.format = *iter;
    }

    {
        selectedConfig.queues.workQueueInfo.queue =
            selectedConfig.device->getQueue(selectedConfig.queues.workQueueInfo.index, 0);
    }

    vk::UniqueDevice& device = selectedConfig.device;

    // Preload shaders
    ShaderRegistry shaderRegistry;
    assert(!shaderRegistry.loadVertexShader(device, ShaderPaths::Simple2D).has_value());
    assert(!shaderRegistry.loadFragmentShader(device, ShaderPaths::ColorPassthrough).has_value());

    auto pipelineBuilder =
        PipelineBuilder()
            .usingConfig(config)
            .usingShaderRegistry(shaderRegistry)
            .usingDevice(selectedConfig.device)
            .withVertexShader(ShaderPaths::Simple2D)
            .withFragmentShader(ShaderPaths::ColorPassthrough)
            .withPrimitiveTopology(PipelineBuilder::PrimitiveTopology::TriangleList)
            .withViewport(PipelineBuilder::Viewport::Fullscreen)
            .withRasterizerState(PipelineBuilder::Rasterizer::BackfaceCulling)
            .withMultisampleState(PipelineBuilder::Multisample::Disabled)
            .withBlendState(PipelineBuilder::Blend::Disabled)
            .withLinearVertexLayout<TriangleVertex>(
                vk::Format::eR32G32Sfloat,
                vk::Format::eR32G32B32Sfloat);

    pipelineBuilder.build(selectedConfig);

    auto swapchainBuilder =
        SwapchainBuilder(config, selectedConfig.surfaceConfig.surface, selectedConfig.device)
            .withBackbufferFormat(selectedConfig.surfaceConfig.format.format)
            .withColorSpace(selectedConfig.surfaceConfig.format.colorSpace)
            .createFramebuffersFor(selectedConfig.pipelineConfig.renderPass);

    assert(!swapchainBuilder.build(selectedConfig.swapchainConfig));

    // Command buffers

    auto [ccpRes, commandPool] = selectedConfig.device->createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = selectedConfig.queues.workQueueInfo.index,
    });
    assert(ccpRes == vk::Result::eSuccess);

    auto [acbRes, commandBuffers] = selectedConfig.device->allocateCommandBuffersUnique({
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
        auto [iasRes, imageAvailable] = selectedConfig.device->createSemaphoreUnique({});
        assert(iasRes == vk::Result::eSuccess);
        imageAvailableList[i] = std::move(imageAvailable);

        auto [rfsRes, renderFinished] = selectedConfig.device->createSemaphoreUnique({});
        assert(rfsRes == vk::Result::eSuccess);
        renderFinishedList[i] = std::move(renderFinished);

        auto [fRes, fence] = selectedConfig.device->createFenceUnique({
            .flags = vk::FenceCreateFlagBits::eSignaled,
        });
        assert(fRes == vk::Result::eSuccess);
        fences[i] = std::move(fence);
    }

    auto memoryProperties = selectedConfig.physicalDevice.getMemoryProperties();

    std::vector<TriangleVertex> vertices = {
        TriangleVertex{{-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        TriangleVertex{{0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        TriangleVertex{{0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},

        TriangleVertex{{-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        TriangleVertex{{0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        TriangleVertex{{0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    };
    auto verticesSize = sizeof(TriangleVertex) * vertices.size();

    auto vertexBuffer = expectResult(Buffer::Builder(selectedConfig.device)
                                         .withVertexBufferFormat()
                                         .withTransferDestFormat(memoryProperties)
                                         .withSize(verticesSize)
                                         .build());
    assert(
        selectedConfig.device
            ->bindBufferMemory(vertexBuffer.buffer.get(), vertexBuffer.memory.get(), 0)
        == vk::Result::eSuccess);

    {
        auto srcBuffer = expectResult(Buffer::Builder(selectedConfig.device)
                                          .withSize(verticesSize)
                                          .withMapFunctionality(memoryProperties)
                                          .withTransferSourceFormat(memoryProperties)
                                          .build());

        assert(
            selectedConfig.device
                ->bindBufferMemory(srcBuffer.buffer.get(), srcBuffer.memory.get(), 0)
            == vk::Result::eSuccess);

        void* data;
        assert(
            selectedConfig.device
                ->mapMemory(srcBuffer.memory.get(), 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &data)
            == vk::Result::eSuccess);
        std::memcpy(data, vertices.data(), verticesSize);
        selectedConfig.device->unmapMemory(srcBuffer.memory.get());

        auto [acb2Res, copyBuffer] = selectedConfig.device->allocateCommandBuffersUnique({
            .commandPool = commandPool.get(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        assert(acb2Res == vk::Result::eSuccess);

        vk::CommandBufferBeginInfo beginInfo = {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };
        assert(copyBuffer[0]->begin(beginInfo) == vk::Result::eSuccess);
        vk::BufferCopy copyInfo{
            .size = verticesSize,
        };
        copyBuffer[0]->copyBuffer(srcBuffer.buffer.get(), vertexBuffer.buffer.get(), 1, &copyInfo);
        assert(copyBuffer[0]->end() == vk::Result::eSuccess);

        vk::SubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &copyBuffer[0].get(),
        };
        assert(
            selectedConfig.queues.workQueueInfo.queue.submit(1, &submitInfo, VK_NULL_HANDLE)
            == vk::Result::eSuccess);

        assert(selectedConfig.queues.workQueueInfo.queue.waitIdle() == vk::Result::eSuccess);
    }

    bool recreateSwapchain = false;
    uint32_t frame = 0;
    uint32_t backbufferFrame = 0;
    while(!mainWindow->shouldClose())
    {
        mainWindow->pollEvents();

        if(windowResized || recreateSwapchain)
        {
            assert(selectedConfig.device->waitIdle() == vk::Result::eSuccess);

            // The GLFW window size and the surface capabilities extents tend to not match, so let's
            // not even do that
            auto val = selectedConfig.physicalDevice
                           .getSurfaceCapabilitiesKHR(*selectedConfig.surfaceConfig.surface)
                           .value;

            auto clampedWidth = std::max(
                val.minImageExtent.width,
                std::min(val.maxImageExtent.width, config.resolutionWidth));
            auto clampedHeight = std::max(
                val.minImageExtent.height,
                std::min(val.maxImageExtent.height, config.resolutionHeight));

            config.resolutionWidth = clampedWidth;
            config.resolutionHeight = clampedHeight;

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

            windowResized = false;

            selectedConfig.pipelineConfig = {};
            selectedConfig.swapchainConfig = {};

            pipelineBuilder.build(selectedConfig);
            swapchainBuilder.createFramebuffersFor(selectedConfig.pipelineConfig.renderPass);

            assert(!swapchainBuilder.build(selectedConfig.swapchainConfig));

            recreateSwapchain = false;

            // The swapchain might already be invalid if the window is still being resized
            continue;
        }

        auto wffRes =
            selectedConfig.device->waitForFences(fences[backbufferFrame].get(), true, UINT64_MAX);
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

        auto [acnRes, swapchainImageIndex] = selectedConfig.device->acquireNextImageKHR(
            *selectedConfig.swapchainConfig.swapchain,
            UINT64_MAX,
            imageAvailableList[backbufferFrame].get(),
            VK_NULL_HANDLE);
        handleError(acnRes);

        selectedConfig.device->resetFences(fences[backbufferFrame].get());

        auto& commandBuffer = commandBuffers[backbufferFrame];
        commandBuffer->reset();
        vk::CommandBufferBeginInfo beginInfo = {};
        assert(commandBuffer->begin(beginInfo) == vk::Result::eSuccess);

        vk::ClearValue clearValue = {std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f})};
        commandBuffer->beginRenderPass(
            {
                .renderPass = selectedConfig.pipelineConfig.renderPass.get(),
                .framebuffer =
                    selectedConfig.swapchainConfig.framebuffers[swapchainImageIndex].get(),
                .renderArea = selectedConfig.pipelineConfig.renderArea,
                .clearValueCount = 1,
                .pClearValues = &clearValue,
            },
            vk::SubpassContents::eInline);
        commandBuffer->bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            *selectedConfig.pipelineConfig.pipeline);
        vk::DeviceSize offset = 0;
        commandBuffer->bindVertexBuffers(0, 1, &vertexBuffer.buffer.get(), &offset);
        commandBuffer->draw(vertices.size(), 1, 0, 0);
        commandBuffer->endRenderPass();
        assert(commandBuffer->end() == vk::Result::eSuccess);

        vk::PipelineStageFlags waitDstStageMask[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
        };
        assert(
            selectedConfig.queues.workQueueInfo.queue.submit(
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
            .pSwapchains = &selectedConfig.swapchainConfig.swapchain.get(),
            .pImageIndices = &swapchainImageIndex,
            .pResults = nullptr,
        };

        handleRetError(selectedConfig.queues.workQueueInfo.queue.presentKHR(presentInfo));

        frame++;
        backbufferFrame = frame % config.backbufferCount;
    }

    assert(selectedConfig.device->waitIdle() == vk::Result::eSuccess);

    glfwTerminate();
    return 0;
}

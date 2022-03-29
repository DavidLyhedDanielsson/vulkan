#pragma once

#include <filesystem>
#include <optional>
#include <tuple>
#include <vulkan/vulkan_raii.hpp>

#include "../config.h"
#include "../shader_registry.h"

class Pipeline
{
  public:
    class Builder
    {
        using Self = Builder&;

      public:
        enum class PrimitiveTopology
        {
            TriangleList
        };

        enum class Viewport
        {
            Fullscreen
        };

        enum class Rasterizer
        {
            BackfaceCulling
        };

        enum class Multisample
        {
            Disabled
        };

        enum class Blend
        {
            Disabled
        };

        Self usingShaderRegistry(const ShaderRegistry&);
        Self usingConfig(const Config&);
        Self usingDevice(vk::UniqueDevice&);

        Self withVertexShader(const std::filesystem::path&);
        Self withFragmentShader(const std::filesystem::path&);

        Self withPrimitiveTopology(PrimitiveTopology);
        Self withViewport(Viewport);
        Self withRasterizerState(Rasterizer);
        Self withMultisampleState(Multisample);
        Self withBlendState(Blend);

        Pipeline build();

      private:
        const ShaderRegistry* shaderRegistry;
        const Config* config;
        vk::UniqueDevice* device;

        std::optional<std::filesystem::path> vertexShaderPathOpt;
        std::optional<std::filesystem::path> fragmentShaderPathOpt;

        PrimitiveTopology primitiveTopology;
        Viewport viewport;
        Rasterizer rasterizer;
        Multisample multisample;
        Blend blend;

        void fillVertexInfo();
        void fillShaderStageInfo();
        void fillInputAssemblyInfo();
        void fillViewportInfo();
        void fillMultisampleInfo();
        void fillBlendInfo();
        void fillRasterizerInfo();
        void fillLayoutInfo();
        void fillRenderPassInfo();

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        vk::PipelineViewportStateCreateInfo viewportInfo;
        vk::Rect2D scissor;
        vk::Viewport vport; // Temp name
        vk::PipelineLayoutCreateInfo layoutInfo;
        vk::PipelineMultisampleStateCreateInfo multisampleInfo;
        vk::PipelineColorBlendAttachmentState blendAttachmentInfo;
        vk::PipelineColorBlendStateCreateInfo blendStateInfo;
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo;
        vk::AttachmentDescription attachmentDescription;
        vk::AttachmentReference attachmentReference;
        vk::SubpassDescription subpassDescription;
        vk::SubpassDependency subpassDependency;
        vk::RenderPassCreateInfo renderPassInfo;
    };

    Pipeline(
        vk::UniquePipeline&& pipeline,
        vk::UniquePipelineLayout&& pipelineLayout,
        vk::UniqueRenderPass&& renderPass,
        vk::Rect2D renderArea);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = default;
    Pipeline& operator=(Pipeline&&) = default;

    void reset();

    vk::UniquePipeline pipeline;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniqueRenderPass renderPass;
    vk::Rect2D renderArea;
};

#pragma once

#include <filesystem>
#include <numeric>
#include <optional>
#include <tuple>
#include <vulkan/vulkan_raii.hpp>

#include "../config.h"
#include "../shader_registry.h"
#include "../stl_utils.h"

class PipelineBuilder
{
    using Self = PipelineBuilder&;

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
    Self usingConfig(const UserConfig&);
    Self usingDevice(vk::UniqueDevice&);

    Self withVertexShader(const std::filesystem::path&);
    Self withFragmentShader(const std::filesystem::path&);

    Self withPrimitiveTopology(PrimitiveTopology);
    Self withViewport(Viewport);
    Self withRasterizerState(Rasterizer);
    Self withMultisampleState(Multisample);
    Self withBlendState(Blend);

    constexpr static uint32_t vkFormatSize(vk::Format format)
    {
        switch(format)
        {
            case vk::Format::eR32Sfloat: return sizeof(float);
            case vk::Format::eR32G32Sfloat: return sizeof(float) * 2;
            case vk::Format::eR32G32B32Sfloat: return sizeof(float) * 3;
            case vk::Format::eR32G32B32A32Sfloat: return sizeof(float) * 4;
            default: return 0;
        }
    }

    template<typename T, typename... VkFormats>
    Self withLinearVertexLayout(VkFormats... formats)
    {
        assert(sizeof(T) == (vkFormatSize(formats) + ...));
        withLinearVertexLayout(formats...);
        return *this;
    }

    template<typename... Rest>
    Self withLinearVertexLayout(vk::Format format, Rest... rest)
    {
        appendLinearVertexLayout(format);
        if constexpr(sizeof...(rest) > 0)
            withLinearVertexLayout(rest...);
        else
            withPerVertexData(
                vertexAttributes.back().offset + vkFormatSize(vertexAttributes.back().format));

        return *this;
    }

    Self withPerVertexData(uint32_t stride)
    {
        this->vertexBinding = {
            .binding = 0,
            .stride = stride,
            .inputRate = vk::VertexInputRate::eVertex,
        };
        return *this;
    }

    Self appendLinearVertexLayout(vk::Format format)
    {
        // Unsupported variable, add it to `vkFormatSize`
        assert(vkFormatSize(format) != 0);

        uint32_t offset = 0;
        if(!vertexAttributes.empty())
            offset = vertexAttributes.back().offset + vkFormatSize(vertexAttributes.back().format);

        this->vertexAttributes.push_back({
            .location = (uint32_t)this->vertexAttributes.size(),
            .binding = 0,
            .format = format,
            .offset = offset,
        });

        return *this;
    }

    void build(SelectedConfig&);

  private:
    vk::VertexInputBindingDescription vertexBinding;
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes;

    const ShaderRegistry* shaderRegistry;
    const UserConfig* config;
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

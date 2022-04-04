#include "pipeline_builder.h"

PipelineBuilder& PipelineBuilder::usingShaderRegistry(const ShaderRegistry& registry)
{
    this->shaderRegistry = &registry;
    return *this;
}

PipelineBuilder& PipelineBuilder::usingConfig(const UserConfig& config)
{
    this->config = &config;
    return *this;
}

PipelineBuilder& PipelineBuilder::usingDevice(vk::UniqueDevice& device)
{
    this->device = &device;
    return *this;
}

PipelineBuilder& PipelineBuilder::withVertexShader(const std::filesystem::path& path)
{
    this->vertexShaderPathOpt = path;
    return *this;
}

PipelineBuilder& PipelineBuilder::withFragmentShader(const std::filesystem::path& path)
{
    this->fragmentShaderPathOpt = path;
    return *this;
}

PipelineBuilder& PipelineBuilder::withPrimitiveTopology(PrimitiveTopology topology)
{
    this->primitiveTopology = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::withViewport(Viewport viewport)
{
    this->viewport = viewport;
    return *this;
}

PipelineBuilder& PipelineBuilder::withRasterizerState(Rasterizer rasterizer)
{
    this->rasterizer = rasterizer;
    return *this;
}

PipelineBuilder& PipelineBuilder::withMultisampleState(Multisample multisample)
{
    this->multisample = multisample;
    return *this;
}

PipelineBuilder& PipelineBuilder::withBlendState(Blend blend)
{
    this->blend = blend;
    return *this;
}

void PipelineBuilder::build(SelectedConfig& config)
{
    fillVertexInfo();
    fillShaderStageInfo();
    fillInputAssemblyInfo();
    fillViewportInfo();
    fillMultisampleInfo();
    fillBlendInfo();
    fillRasterizerInfo();
    fillLayoutInfo();
    fillRenderPassInfo();

    auto [cplRes, pipelineLayout] = (*device)->createPipelineLayoutUnique(layoutInfo);
    assert(cplRes == vk::Result::eSuccess);

    auto [crpRes, renderPass] = (*device)->createRenderPassUnique(renderPassInfo);
    assert(crpRes == vk::Result::eSuccess);

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo = {
        .stageCount = (uint32_t)shaderStages.size(),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pTessellationState = nullptr,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &rasterizerInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &blendStateInfo,
        .pDynamicState = nullptr,
        .layout = pipelineLayout.get(),
        .renderPass = renderPass.get(),
        .subpass = 0,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = -1,
    };

    auto [cgpRes, pipeline] =
        (*device)->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipelineCreateInfo);
    assert(cgpRes == vk::Result::eSuccess);

    vk::Rect2D renderArea = {
        .offset = {(int32_t)vport.x, (int32_t)vport.y},
        .extent = {(uint32_t)vport.width, (uint32_t)vport.height}};

    config.pipelineConfig.pipeline = std::move(pipeline);
    config.pipelineConfig.layout = std::move(pipelineLayout);
    config.pipelineConfig.renderPass = std::move(renderPass);
    config.pipelineConfig.renderArea = renderArea;
}

void PipelineBuilder::fillVertexInfo()
{
    if(this->vertexAttributes.empty())
    {
        vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };
    }
    else
    {
        vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &this->vertexBinding,
            .vertexAttributeDescriptionCount = (uint32_t)this->vertexAttributes.size(),
            .pVertexAttributeDescriptions = this->vertexAttributes.data(),
        };
    }
}

void PipelineBuilder::fillShaderStageInfo()
{
    if(vertexShaderPathOpt.has_value() && fragmentShaderPathOpt.has_value())
    {
        auto vertexShader = shaderRegistry->getVertexShader(vertexShaderPathOpt.value());
        vk::PipelineShaderStageCreateInfo vertexShaderStageInfo = {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertexShader->shaderModule.get(),
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };

        auto fragmentShader = shaderRegistry->getFragmentShader(fragmentShaderPathOpt.value());
        vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo = {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragmentShader->shaderModule.get(),
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };

        shaderStages = {vertexShaderStageInfo, fragmentShaderStageInfo};
        return;
    }

    assert(false);
}

void PipelineBuilder::fillInputAssemblyInfo()
{
    if(primitiveTopology == PrimitiveTopology::TriangleList)
    {
        inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false,
        };
        return;
    }
    assert(false);
}

void PipelineBuilder::fillViewportInfo()
{
    if(viewport == Viewport::Fullscreen)
    {
        vport = vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)config->resolutionWidth,
            .height = (float)config->resolutionHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        scissor = vk::Rect2D{
            .offset = {0, 0},
            .extent = {config->resolutionWidth, config->resolutionHeight},
        };
        viewportInfo = vk::PipelineViewportStateCreateInfo{
            .viewportCount = 1,
            .pViewports = &vport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };
        return;
    }
    assert(false);
}

void PipelineBuilder::fillMultisampleInfo()
{
    if(multisample == Multisample::Disabled)
    {
        multisampleInfo = vk::PipelineMultisampleStateCreateInfo{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false,
        };
        return;
    }
    assert(false);
}

void PipelineBuilder::fillBlendInfo()
{
    if(blend == Blend::Disabled)
    {
        blendAttachmentInfo = vk::PipelineColorBlendAttachmentState{
            .blendEnable = false,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                              | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        blendStateInfo = vk::PipelineColorBlendStateCreateInfo{
            .logicOpEnable = false,
            .logicOp = vk::LogicOp::eOr,
            .attachmentCount = 1,
            .pAttachments = &blendAttachmentInfo,
            .blendConstants = vk::ArrayWrapper1D<float, 4>({0.0f, 0.0f, 0.0f, 0.0f}),
        };
        return;
    }
    assert(false);
}

void PipelineBuilder::fillRasterizerInfo()
{
    if(rasterizer == Rasterizer::BackfaceCulling)
    {
        rasterizerInfo = vk::PipelineRasterizationStateCreateInfo{
            .depthClampEnable = false,
            .rasterizerDiscardEnable = false,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = false,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f,
        };
        return;
    }
    assert(false);
}

void PipelineBuilder::fillLayoutInfo()
{
    layoutInfo = vk::PipelineLayoutCreateInfo{
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
}

void PipelineBuilder::fillRenderPassInfo()
{
    attachmentDescription = {
        .format = config->backbufferFormat,
        .samples = config->sampleCount,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };
    attachmentReference = {
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };
    subpassDescription = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentReference,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dependencyFlags = vk::DependencyFlags(),
    };
    renderPassInfo = vk::RenderPassCreateInfo{
        .attachmentCount = 1,
        .pAttachments = &attachmentDescription,
        .subpassCount = 1,
        .pSubpasses = &subpassDescription,
        .dependencyCount = 1,
        .pDependencies = &subpassDependency,
    };
}

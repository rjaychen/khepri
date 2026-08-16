#include "Pipeline.h"
#include "../core/Logger.h"
#include <stdexcept>

PipelineBuilder::PipelineBuilder() {
    SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    SetPolygonMode(VK_POLYGON_MODE_FILL);
    SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    SetMultisamplingNone();
    DisableBlending();
    EnableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
}

PipelineBuilder& PipelineBuilder::SetShaders(VkShaderModule vertShader, VkShaderModule fragShader) {
    m_vertShader = vertShader;
    m_fragShader = fragShader;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                                                const std::vector<VkVertexInputAttributeDescription>& attributes) {
    m_vertexBindings = bindings;
    m_vertexAttributes = attributes;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology) {
    m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_inputAssembly.topology = topology;
    m_inputAssembly.primitiveRestartEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetPolygonMode(VkPolygonMode mode) {
    m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_rasterizer.depthClampEnable = VK_FALSE;
    m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
    m_rasterizer.polygonMode = mode;
    m_rasterizer.lineWidth = 1.0f;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
    m_rasterizer.cullMode = cullMode;
    m_rasterizer.frontFace = frontFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisamplingNone() {
    return SetMultisampling(VK_SAMPLE_COUNT_1_BIT, false);
}

PipelineBuilder& PipelineBuilder::SetMultisampling(VkSampleCountFlagBits samples, bool sampleShading) {
    m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_multisampling.sampleShadingEnable = sampleShading ? VK_TRUE : VK_FALSE;
    m_multisampling.rasterizationSamples = samples;
    m_multisampling.minSampleShading = sampleShading ? 0.2f : 1.0f;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::DisableBlending() {
    m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.blendEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetColorAttachmentFormat(VkFormat format) {
    m_colorAttachmentFormat = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthFormat(VkFormat format) {
    m_depthAttachmentFormat = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op) {
    m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_depthStencil.depthTestEnable = VK_TRUE;
    m_depthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
    m_depthStencil.depthCompareOp = op;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    return *this;
}

VkPipeline PipelineBuilder::Build(VulkanContext& context, VkPipelineLayout layout) {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    if (m_vertShader != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = m_vertShader;
        vertStage.pName = "main";
        shaderStages.push_back(vertStage);
    }

    if (m_fragShader != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = m_fragShader;
        fragStage.pName = "main";
        shaderStages.push_back(fragStage);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = m_vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = m_vertexAttributes.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_colorBlendAttachment;

    // Vulkan 1.3 Dynamic Rendering Pipeline Creation Structure
    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &m_colorAttachmentFormat;
    renderingCreateInfo.depthAttachmentFormat = m_depthAttachmentFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingCreateInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE; // Using Vulkan 1.3 Dynamic Rendering!

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(context.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline!");
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    return pipeline;
}

VkShaderModule PipelineBuilder::CreateShaderModule(VulkanContext& context, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context.GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module!");
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

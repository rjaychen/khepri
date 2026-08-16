#pragma once

#include <volk.h>
#include <vector>
#include <string>
#include "VulkanContext.h"

class PipelineBuilder {
public:
    PipelineBuilder();

    PipelineBuilder& SetShaders(VkShaderModule vertShader, VkShaderModule fragShader);
    PipelineBuilder& SetVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                                   const std::vector<VkVertexInputAttributeDescription>& attributes);
    PipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);
    PipelineBuilder& SetPolygonMode(VkPolygonMode mode);
    PipelineBuilder& SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& SetMultisamplingNone();
    PipelineBuilder& SetMultisampling(VkSampleCountFlagBits samples, bool sampleShading = false);
    PipelineBuilder& DisableBlending();
    PipelineBuilder& SetColorAttachmentFormat(VkFormat format);
    PipelineBuilder& SetDepthFormat(VkFormat format);
    PipelineBuilder& EnableDepthTest(bool depthWriteEnable, VkCompareOp op);

    VkPipeline Build(VulkanContext& context, VkPipelineLayout layout);

    static VkShaderModule CreateShaderModule(VulkanContext& context, const std::vector<uint32_t>& code);

private:
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

    std::vector<VkVertexInputBindingDescription> m_vertexBindings;
    std::vector<VkVertexInputAttributeDescription> m_vertexAttributes;

    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
    VkPipelineRasterizationStateCreateInfo m_rasterizer{};
    VkPipelineColorBlendAttachmentState m_colorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo m_multisampling{};
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{};

    VkFormat m_colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthAttachmentFormat = VK_FORMAT_UNDEFINED;
};

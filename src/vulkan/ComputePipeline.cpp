#include "ComputePipeline.h"
#include "../core/Logger.h"
#include <fstream>
#include <stdexcept>

namespace khepri {

static std::vector<uint32_t> LoadSPIRVFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: " + path);
        return {};
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();
    return buffer;
}

ComputePipeline::ComputePipeline(VulkanContext& context, const std::string& shaderPath, uint32_t pushConstantSize)
    : m_context(&context), m_pushConstantSize(pushConstantSize) {
    CreateDescriptorSetLayout();
    CreatePipeline(shaderPath, pushConstantSize);
}

ComputePipeline::~ComputePipeline() {
    VkDevice device = m_context->GetDevice();
    if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_layout, nullptr);
    if (m_descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
}

void ComputePipeline::CreateDescriptorSetLayout() {
    if (!m_context || m_context->GetDevice() == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(m_context->GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute descriptor set layout");
    }
}

void ComputePipeline::CreatePipeline(const std::string& shaderPath, uint32_t pushConstantSize) {
    if (!m_context || m_context->GetDevice() == VK_NULL_HANDLE) {
        return;
    }

    auto code = LoadSPIRVFile(shaderPath);
    if (code.empty()) {
        LOG_WARN("Compute shader code empty for: " + shaderPath);
        return;
    }

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = code.size() * sizeof(uint32_t);
    moduleInfo.pCode = code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_context->GetDevice(), &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute shader module");
        return;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

    VkPushConstantRange pushConstantRange{};
    if (pushConstantSize > 0) {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = pushConstantSize;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    if (vkCreatePipelineLayout(m_context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline layout");
        vkDestroyShaderModule(m_context->GetDevice(), shaderModule, nullptr);
        return;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_layout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";

    if (vkCreateComputePipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline");
    } else {
        LOG_INFO("Successfully created compute pipeline for: " + shaderPath);
    }

    vkDestroyShaderModule(m_context->GetDevice(), shaderModule, nullptr);
}

void ComputePipeline::Dispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ,
                               const void* pushConstantData, VkDescriptorSet descriptorSet) {
    if (m_pipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    if (descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &descriptorSet, 0, nullptr);
    }

    if (m_pushConstantSize > 0 && pushConstantData != nullptr) {
        vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_pushConstantSize, pushConstantData);
    }

    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}

} // namespace khepri

#pragma once

#include "VulkanContext.h"
#include <volk.h>
#include <string>
#include <vector>

namespace khepri {

class ComputePipeline {
public:
    ComputePipeline(VulkanContext& context, const std::string& shaderPath, uint32_t pushConstantSize = 0);
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    void Dispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, const void* pushConstantData = nullptr);

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_layout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    void CreateDescriptorSetLayout();
    void CreatePipeline(const std::string& shaderPath, uint32_t pushConstantSize);

    VulkanContext* m_context = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    uint32_t m_pushConstantSize = 0;
};

} // namespace khepri

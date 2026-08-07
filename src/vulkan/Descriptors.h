#pragma once

#include <volk.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "VulkanContext.h"

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count = 1);
    VkDescriptorSetLayout Build(VulkanContext& context);

private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

class DescriptorAllocator {
public:
    DescriptorAllocator(VulkanContext& context, uint32_t maxSets = 100);
    ~DescriptorAllocator();

    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

    VkDescriptorSet Allocate(VkDescriptorSetLayout layout);
    void Reset();

private:
    VulkanContext& m_context;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};

class DescriptorWriter {
public:
    DescriptorWriter& WriteBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    DescriptorWriter& WriteImage(uint32_t binding, VkDescriptorType type, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout);
    void UpdateSet(VulkanContext& context, VkDescriptorSet set);

private:
    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkDescriptorBufferInfo> m_bufferInfos;
    std::vector<VkDescriptorImageInfo> m_imageInfos;
};

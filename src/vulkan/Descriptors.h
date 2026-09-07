#pragma once

#include <volk.h>
#include <vector>
#include <deque>
#include <unordered_map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "../core/Logger.h"

class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count = 1);
    DescriptorLayoutBuilder& AddBindings(std::span<const VkDescriptorSetLayoutBinding> bindings);
    void Clear() noexcept { m_bindings.clear(); }
    [[nodiscard]] const std::vector<VkDescriptorSetLayoutBinding>& GetBindings() const noexcept { return m_bindings; }

    VkDescriptorSetLayout Build(VulkanContext& context);

    static VkDescriptorSetLayout Build(VulkanContext& context, std::span<const VkDescriptorSetLayoutBinding> bindings);

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

// NOTE [DescriptorWriter Reference Stability Requirement]:
// DescriptorWriter uses std::deque<VkDescriptorBufferInfo> and std::deque<VkDescriptorImageInfo>
// specifically because std::deque guarantees pointer and reference stability across insertions (push_back).
// VkWriteDescriptorSet stores raw pointers (pBufferInfo, pImageInfo) to elements in these containers.
// A std::vector would reallocate contiguous storage when growing, dangling the pointers passed to Vulkan
// and causing severe memory corruption during vkUpdateDescriptorSets.
class DescriptorWriter {
public:
    DescriptorWriter& WriteBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    DescriptorWriter& WriteImage(uint32_t binding, VkDescriptorType type, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout);
    void UpdateSet(VulkanContext& context, VkDescriptorSet set);

    [[nodiscard]] size_t GetWriteCount() const noexcept { return m_writes.size(); }

private:
    std::vector<VkWriteDescriptorSet> m_writes;
    std::deque<VkDescriptorBufferInfo> m_bufferInfos;
    std::deque<VkDescriptorImageInfo> m_imageInfos;
};


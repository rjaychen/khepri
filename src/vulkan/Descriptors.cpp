#include "Descriptors.h"
#include "../core/Logger.h"
#include <stdexcept>

// --- DescriptorLayoutBuilder ---
DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = count;
    b.stageFlags = stageFlags;
    b.pImmutableSamplers = nullptr;
    m_bindings.push_back(b);
    return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBindings(std::span<const VkDescriptorSetLayoutBinding> bindings) {
    m_bindings.insert(m_bindings.end(), bindings.begin(), bindings.end());
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VulkanContext& context) {
    return Build(context, m_bindings);
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VulkanContext& context, std::span<const VkDescriptorSetLayoutBinding> bindings) {
    if (context.GetDevice() == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(context.GetDevice(), &info, nullptr, &layout),
                    "Failed to create descriptor set layout");
    return layout;
}

// --- DescriptorAllocator ---
DescriptorAllocator::DescriptorAllocator(VulkanContext& context, uint32_t maxSets)
    : m_context(context) {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(m_context.GetDevice(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor pool!");
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

DescriptorAllocator::~DescriptorAllocator() {
    if (m_pool) {
        vkDestroyDescriptorPool(m_context.GetDevice(), m_pool, nullptr);
    }
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(m_context.GetDevice(), &allocInfo, &set) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate descriptor set!");
        throw std::runtime_error("Failed to allocate descriptor set!");
    }
    return set;
}

void DescriptorAllocator::Reset() {
    vkResetDescriptorPool(m_context.GetDevice(), m_pool, 0);
}

// --- DescriptorWriter ---
DescriptorWriter& DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = size;
    m_bufferInfos.push_back(bufferInfo);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &m_bufferInfos.back();
    m_writes.push_back(write);

    return *this;
}

DescriptorWriter& DescriptorWriter::WriteImage(uint32_t binding, VkDescriptorType type, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;
    imageInfo.imageLayout = imageLayout;
    m_imageInfos.push_back(imageInfo);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &m_imageInfos.back();
    m_writes.push_back(write);

    return *this;
}

void DescriptorWriter::UpdateSet(VulkanContext& context, VkDescriptorSet set) {
    for (auto& write : m_writes) {
        write.dstSet = set;
    }
    vkUpdateDescriptorSets(context.GetDevice(), static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

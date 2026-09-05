#include "Buffer.h"
#include "VulkanContext.h"
#include "../core/Logger.h"
#include <stdexcept>

Buffer::Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags)
    : m_context(&context), m_size(size) {

    const VkDeviceSize allocSize = std::max(size, static_cast<VkDeviceSize>(16));

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = flags;
    allocInfo.usage = memoryUsage;

    if (vmaCreateBuffer(context.GetAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, &m_allocationInfo) != VK_SUCCESS) {
        LOG_ERROR("Failed to create VMA Buffer of size " + std::to_string(size));
        throw std::runtime_error("Failed to create VMA Buffer");
    }

    if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        m_mappedData = m_allocationInfo.pMappedData;
    }
}

Buffer::~Buffer() {
    if (m_buffer && m_context) {
        vmaDestroyBuffer(m_context->GetAllocator(), m_buffer, m_allocation);
    }
}

Buffer::Buffer(Buffer&& other) noexcept {
    m_context = other.m_context;
    m_buffer = other.m_buffer;
    m_allocation = other.m_allocation;
    m_allocationInfo = other.m_allocationInfo;
    m_size = other.m_size;
    m_mappedData = other.m_mappedData;

    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_size = 0;
    other.m_mappedData = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (m_buffer && m_context) {
            vmaDestroyBuffer(m_context->GetAllocator(), m_buffer, m_allocation);
        }

        m_context = other.m_context;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_allocationInfo = other.m_allocationInfo;
        m_size = other.m_size;
        m_mappedData = other.m_mappedData;

        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_mappedData = nullptr;
    }
    return *this;
}

void Buffer::Map() {
    if (!m_mappedData && m_context) {
        vmaMapMemory(m_context->GetAllocator(), m_allocation, &m_mappedData);
    }
}

void Buffer::Unmap() {
    if (m_mappedData && m_context) {
        vmaUnmapMemory(m_context->GetAllocator(), m_allocation);
        m_mappedData = nullptr;
    }
}

void Buffer::CopyToBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!data || size == 0) return;
    if (offset + size > m_size) {
        LOG_ERROR("Buffer copy out of bounds");
        return;
    }

    if (m_mappedData) {
        memcpy(static_cast<char*>(m_mappedData) + offset, data, size);
    } else {
        Map();
        memcpy(static_cast<char*>(m_mappedData) + offset, data, size);
        Unmap();
    }
}

void Buffer::CopyBuffer(VulkanContext& context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    if (!srcBuffer || !dstBuffer || size == 0) return;

    context.ImmediateSubmit([&](VkCommandBuffer commandBuffer) {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    });
}

#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstring>

class VulkanContext;

class Buffer {
public:
    Buffer(VulkanContext& context, 
           VkDeviceSize size, 
           VkBufferUsageFlags usage, 
           VmaMemoryUsage memoryUsage,
           VmaAllocationCreateFlags flags = 0);

    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    VkBuffer GetBuffer() const { return m_buffer; }
    VmaAllocation GetAllocation() const { return m_allocation; }
    VkDeviceSize GetSize() const { return m_size; }
    void* GetMappedData() const { return m_mappedData; }

    void Map();
    void Unmap();
    void CopyToBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    static void CopyBuffer(VulkanContext& context, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

private:
    VulkanContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocationInfo{};
    VkDeviceSize m_size = 0;
    void* m_mappedData = nullptr;
};

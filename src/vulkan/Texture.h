#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <memory>
#include <string>
#include "VulkanContext.h"
#include "Descriptors.h"

class Texture {
public:
    Texture(VulkanContext& context, uint32_t width, uint32_t height, const unsigned char* pixels,
            VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    static std::shared_ptr<Texture> CreateFromMemory(VulkanContext& context, const uint8_t* data, size_t size,
                                                      VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator);
    static std::shared_ptr<Texture> CreateFromFile(VulkanContext& context, const std::string& filepath,
                                                    VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator);
    static std::shared_ptr<Texture> CreateWhiteTexture(VulkanContext& context,
                                                       VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator);

private:
    void CreateTextureImage(const unsigned char* pixels);
    void CreateImageView();
    void CreateSampler();
    void CreateDescriptorSet(VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator);

    VulkanContext& m_context;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};

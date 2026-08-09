#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include "Buffer.h"
#include "../core/Logger.h"
#include <stdexcept>
#include <cstring>

Texture::Texture(VulkanContext& context, uint32_t width, uint32_t height, const unsigned char* pixels,
                 VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator, VkBuffer lightUBOBuffer)
    : m_context(context), m_width(width), m_height(height) {
    if (width == 0 || height == 0 || !pixels) {
        throw std::runtime_error("Invalid texture dimensions or null pixel data!");
    }

    CreateTextureImage(pixels);
    CreateImageView();
    CreateSampler();
    CreateDescriptorSet(setLayout, allocator, lightUBOBuffer);
}

Texture::~Texture() {
    VkDevice device = m_context.GetDevice();
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_context.GetAllocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_context(other.m_context),
      m_width(other.m_width),
      m_height(other.m_height),
      m_image(other.m_image),
      m_allocation(other.m_allocation),
      m_imageView(other.m_imageView),
      m_sampler(other.m_sampler),
      m_descriptorSet(other.m_descriptorSet) {
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_sampler != VK_NULL_HANDLE) vkDestroySampler(m_context.GetDevice(), m_sampler, nullptr);
        if (m_imageView != VK_NULL_HANDLE) vkDestroyImageView(m_context.GetDevice(), m_imageView, nullptr);
        if (m_image != VK_NULL_HANDLE) vmaDestroyImage(m_context.GetAllocator(), m_image, m_allocation);

        m_width = other.m_width;
        m_height = other.m_height;
        m_image = other.m_image;
        m_allocation = other.m_allocation;
        m_imageView = other.m_imageView;
        m_sampler = other.m_sampler;
        m_descriptorSet = other.m_descriptorSet;

        other.m_image = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_descriptorSet = VK_NULL_HANDLE;
    }
    return *this;
}

void Texture::CreateTextureImage(const unsigned char* pixels) {
    VkDeviceSize imageSize = m_width * m_height * 4;

    Buffer stagingBuffer(
        m_context,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    stagingBuffer.CopyToBuffer(pixels, imageSize);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_width;
    imageInfo.extent.height = m_height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(m_context.GetAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture image!");
        throw std::runtime_error("Failed to create texture image!");
    }

    // Transition layout & copy buffer to image
    m_context.ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};

        vkCmdCopyBufferToImage(cmd, stagingBuffer.GetBuffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    });
}

void Texture::CreateImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture image view!");
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void Texture::CreateSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture sampler!");
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

void Texture::CreateDescriptorSet(VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator, VkBuffer lightUBOBuffer) {
    m_descriptorSet = allocator.Allocate(setLayout);

    DescriptorWriter writer;
    writer.WriteImage(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_imageView, m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (lightUBOBuffer != VK_NULL_HANDLE) {
        writer.WriteBuffer(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, lightUBOBuffer, sizeof(LightUBO));
    }
    writer.UpdateSet(m_context, m_descriptorSet);
}

std::shared_ptr<Texture> Texture::CreateFromMemory(VulkanContext& context, const uint8_t* data, size_t size,
                                                  VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator, VkBuffer lightUBOBuffer) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_WARN("Failed to decode image from memory using stb_image. Falling back to default white texture.");
        return CreateWhiteTexture(context, setLayout, allocator, lightUBOBuffer);
    }

    auto texture = std::make_shared<Texture>(context, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                            pixels, setLayout, allocator, lightUBOBuffer);
    stbi_image_free(pixels);
    return texture;
}

std::shared_ptr<Texture> Texture::CreateFromFile(VulkanContext& context, const std::string& filepath,
                                                VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator, VkBuffer lightUBOBuffer) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_WARN("Failed to load image file from disk: " + filepath + ". Falling back to white texture.");
        return CreateWhiteTexture(context, setLayout, allocator, lightUBOBuffer);
    }

    auto texture = std::make_shared<Texture>(context, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                            pixels, setLayout, allocator, lightUBOBuffer);
    stbi_image_free(pixels);
    return texture;
}

std::shared_ptr<Texture> Texture::CreateWhiteTexture(VulkanContext& context,
                                                   VkDescriptorSetLayout setLayout, DescriptorAllocator& allocator, VkBuffer lightUBOBuffer) {
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    return std::make_shared<Texture>(context, 1, 1, whitePixel, setLayout, allocator, lightUBOBuffer);
}

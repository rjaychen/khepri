#pragma once

#include <volk.h>
#include <imgui.h>
#include <memory>
#include "../vulkan/VulkanContext.h"
#include "../scene/Camera.h"
#include "../vulkan/Buffer.h"

class ViewportPanel {
public:
    ViewportPanel(VulkanContext& context);
    ~ViewportPanel();

    void CreateFramebuffer(uint32_t width, uint32_t height);
    void RenderUI(Camera& camera, VkDescriptorSet viewportTextureDS);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool IsFocused() const { return m_isFocused; }
    bool IsHovered() const { return m_isHovered; }

    VkImageView GetColorImageView() const { return m_colorImageView; }
    VkImageView GetDepthImageView() const { return m_depthImageView; }
    VkSampler GetSampler() const { return m_sampler; }
    VkImage GetColorImage() const { return m_colorImage; }
    VkImage GetDepthImage() const { return m_depthImage; }

    void TransitionToShaderRead(VkCommandBuffer cmd);
    void TransitionToColorAttachment(VkCommandBuffer cmd);

private:
    VulkanContext& m_context;
    uint32_t m_width = 800;
    uint32_t m_height = 600;
    bool m_isFocused = false;
    bool m_isHovered = false;

    VkImage m_colorImage = VK_NULL_HANDLE;
    VmaAllocation m_colorImageAllocation = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
};

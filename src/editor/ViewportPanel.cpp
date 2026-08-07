#include "ViewportPanel.h"
#include "../core/Logger.h"
#include <stdexcept>

ViewportPanel::ViewportPanel(VulkanContext& context)
    : m_context(context) {
    
    // Create Sampler for ImGui texture sampling
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create viewport sampler");
        throw std::runtime_error("Failed to create viewport sampler");
    }

    CreateFramebuffer(m_width, m_height);
}

ViewportPanel::~ViewportPanel() {
    if (m_sampler) vkDestroySampler(m_context.GetDevice(), m_sampler, nullptr);
    if (m_colorImageView) vkDestroyImageView(m_context.GetDevice(), m_colorImageView, nullptr);
    if (m_colorImage) vmaDestroyImage(m_context.GetAllocator(), m_colorImage, m_colorImageAllocation);
    if (m_depthImageView) vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr);
    if (m_depthImage) vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation);
}

void ViewportPanel::CreateFramebuffer(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    if (m_colorImageView) vkDestroyImageView(m_context.GetDevice(), m_colorImageView, nullptr);
    if (m_colorImage) vmaDestroyImage(m_context.GetAllocator(), m_colorImage, m_colorImageAllocation);
    if (m_depthImageView) vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr);
    if (m_depthImage) vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation);

    m_width = width;
    m_height = height;

    // Color Image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = { m_width, m_height, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateImage(m_context.GetAllocator(), &imgInfo, &allocInfo, &m_colorImage, &m_colorImageAllocation, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_colorImageView);

    // Depth Image
    VkImageCreateInfo depthInfo = imgInfo;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vmaCreateImage(m_context.GetAllocator(), &depthInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr);

    viewInfo.image = m_depthImage;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_depthImageView);
}

void ViewportPanel::RenderUI(Camera& camera, VkDescriptorSet viewportTextureDS) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_isFocused = ImGui::IsWindowFocused();
    m_isHovered = ImGui::IsWindowHovered();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if ((uint32_t)viewportSize.x != m_width || (uint32_t)viewportSize.y != m_height) {
        if (viewportSize.x > 0 && viewportSize.y > 0) {
            CreateFramebuffer((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
            camera.SetViewportSize(viewportSize.x, viewportSize.y);
        }
    }

    if (viewportTextureDS != VK_NULL_HANDLE) {
        ImGui::Image((ImTextureID)viewportTextureDS, viewportSize);
    }

    // Viewport Interactive Camera Input Controls
    if (m_isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        camera.Orbit(dragDelta.x, dragDelta.y);
    }
    if (m_isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        camera.Pan(dragDelta.x, dragDelta.y);
    }
    if (m_isHovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            camera.Zoom(wheel);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::TransitionToColorAttachment(VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_colorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ViewportPanel::TransitionToShaderRead(VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_colorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

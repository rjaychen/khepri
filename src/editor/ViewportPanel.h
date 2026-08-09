#pragma once

#include <volk.h>
#include <imgui.h>
#include <memory>
#include "../vulkan/VulkanContext.h"
#include "../scene/Camera.h"
class MeshComponent;
class SceneNode;

class ViewportPanel {
public:
    ViewportPanel(VulkanContext& context);
    ~ViewportPanel();


    enum class ResolutionMode {
        FitPanel = 0,
        Scale50,
        Scale75,
        Scale100,
        Scale150,
        Scale200,
        Fixed720p,
        Fixed1080p,
        Fixed1440p
    };

    void CreateFramebuffer(uint32_t width, uint32_t height);
    void RenderUI(Camera& camera, VkDescriptorSet& viewportTextureDS, float deltaTime = 0.016f, const SceneNode* selectedNode = nullptr, const SceneNode* rootNode = nullptr);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    ResolutionMode GetResolutionMode() const { return m_resMode; }
    void SetResolutionMode(ResolutionMode mode) { m_resMode = mode; }
    bool IsFocused() const { return m_isFocused; }
    bool IsHovered() const { return m_isHovered; }

    VkImageView GetColorImageView() const { return m_colorImageView; }
    VkImageView GetDepthImageView() const { return m_depthImageView; }
    VkSampler GetSampler() const { return m_sampler; }
    VkImage GetColorImage() const { return m_colorImage; }
    VkImage GetDepthImage() const { return m_depthImage; }

    void TransitionToShaderRead(VkCommandBuffer cmd);
    void TransitionToColorAttachment(VkCommandBuffer cmd);

    void SetNativeWindow(GLFWwindow* window) { m_window = window; }

private:
    VulkanContext& m_context;
    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 800;
    uint32_t m_height = 600;
    ResolutionMode m_resMode = ResolutionMode::FitPanel;
    bool m_isFocused = false;
    bool m_isHovered = false;
    bool m_cursorLocked = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    VkImage m_colorImage = VK_NULL_HANDLE;
    VmaAllocation m_colorImageAllocation = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
};

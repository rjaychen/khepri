#pragma once

#include <volk.h>
#include <imgui.h>
#include <memory>
#include "../vulkan/VulkanContext.h"
#include "../scene/Camera.h"
class MeshComponent;
class SceneNode;

#include <functional>

#include "TransformGizmo.h"
#include "ViewCube.h"
#include "LightVisualizer.h"

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

    khepri::TransformGizmo& GetGizmo() noexcept { return m_gizmo; }
    const khepri::TransformGizmo& GetGizmo() const noexcept { return m_gizmo; }
    khepri::ViewCube& GetViewCube() noexcept { return m_viewCube; }
    const khepri::ViewCube& GetViewCube() const noexcept { return m_viewCube; }

    [[nodiscard]] khepri::LightHelperDisplayMode GetLightHelperMode() const noexcept { return m_lightHelperMode; }
    void SetLightHelperMode(khepri::LightHelperDisplayMode mode) noexcept { m_lightHelperMode = mode; }

    VkImageView GetColorImageView() const { return m_colorImageView; }
    VkImageView GetDepthImageView() const { return m_depthImageView; }
    VkImageView GetMSAAColorImageView() const { return m_msaaColorImageView; }
    VkImageView GetMSAADepthImageView() const { return m_msaaDepthImageView; }
    VkSampleCountFlagBits GetMSAASamples() const { return m_msaaSamples; }
    void SetMSAASamples(VkSampleCountFlagBits samples);

    VkSampler GetSampler() const { return m_sampler; }
    VkImage GetColorImage() const { return m_colorImage; }
    VkImage GetDepthImage() const { return m_depthImage; }

    void TransitionToShaderRead(VkCommandBuffer cmd);
    void TransitionToColorAttachment(VkCommandBuffer cmd);

    void SetNativeWindow(GLFWwindow* window) { m_window = window; }
    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }
    void SetSelectNodeCallback(std::function<void(SceneNode*)> cb) { m_onSelectNode = std::move(cb); }
    [[nodiscard]] bool IsLightIconHovered() const noexcept { return m_hoveredLightNode != nullptr; }

private:
    VulkanContext& m_context;
    GLFWwindow* m_window = nullptr;
    khepri::TransformGizmo m_gizmo;
    khepri::ViewCube m_viewCube;
    khepri::LightHelperDisplayMode m_lightHelperMode = khepri::LightHelperDisplayMode::Selected;
    const SceneNode* m_hoveredLightNode = nullptr;
    std::function<void(SceneNode*)> m_onSelectNode;
    uint32_t m_width = 800;
    uint32_t m_height = 600;
    ResolutionMode m_resMode = ResolutionMode::FitPanel;
    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_8_BIT; // Default: 8x MSAA (Best Anti-Aliasing!)
    bool m_needTextureUpdate = false;
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

    VkImage m_msaaColorImage = VK_NULL_HANDLE;
    VmaAllocation m_msaaColorImageAllocation = VK_NULL_HANDLE;
    VkImageView m_msaaColorImageView = VK_NULL_HANDLE;

    VkImage m_msaaDepthImage = VK_NULL_HANDLE;
    VmaAllocation m_msaaDepthImageAllocation = VK_NULL_HANDLE;
    VkImageView m_msaaDepthImageView = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
    std::function<void(const std::string&)> m_onImportModel;

    ImVec2 m_clickStartPos{0.0f, 0.0f};
    bool m_isPotentialClick = false;

    SceneNode* RaycastScene(const Camera& camera, const SceneNode* rootNode, const glm::vec2& mousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize);
};

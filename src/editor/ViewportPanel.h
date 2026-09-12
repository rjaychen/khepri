#pragma once

#include <volk.h>
#include <imgui.h>
#include <memory>
#include <array>
#include <functional>
#include "../vulkan/VulkanContext.h"
#include "../scene/Camera.h"

class MeshComponent;
class SceneNode;

#include "TransformGizmo.h"
#include "ViewCube.h"
#include "LightVisualizer.h"

struct FramebufferAttachment {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;

    void Destroy(VkDevice device, VmaAllocator allocator);
};

struct RenderTargetFrame {
    FramebufferAttachment color;
    FramebufferAttachment depth;
    FramebufferAttachment msaaColor;
    FramebufferAttachment msaaDepth;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    void Destroy(VkDevice device, VmaAllocator allocator);
};

class ViewportPanel {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

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
    void RenderUI(Camera& camera, uint32_t currentFrame, float deltaTime = 0.016f, const SceneNode* selectedNode = nullptr, const SceneNode* rootNode = nullptr);
    void RenderUI(Camera& camera, VkDescriptorSet& legacyDS, float deltaTime = 0.016f, const SceneNode* selectedNode = nullptr, const SceneNode* rootNode = nullptr);

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

    // 3D Ground Grid Controls
    [[nodiscard]] bool IsGridVisible() const noexcept { return m_showGrid; }
    void SetGridVisible(bool visible) noexcept { m_showGrid = visible; }
    void ToggleGrid() noexcept { m_showGrid = !m_showGrid; }
    [[nodiscard]] float GetGridCellSize() const noexcept { return m_gridCellSize; }
    void SetGridCellSize(float size) noexcept { m_gridCellSize = std::max(0.1f, size); }
    [[nodiscard]] float GetGridMajorStep() const noexcept { return m_gridMajorStep; }
    void SetGridMajorStep(float step) noexcept { m_gridMajorStep = std::max(1.0f, step); }
    [[nodiscard]] float GetGridFadeDistance() const noexcept { return m_gridFadeDistance; }
    void SetGridFadeDistance(float dist) noexcept { m_gridFadeDistance = std::max(5.0f, dist); }
    [[nodiscard]] float GetGridOpacity() const noexcept { return m_gridOpacity; }
    void SetGridOpacity(float opacity) noexcept { m_gridOpacity = std::clamp(opacity, 0.0f, 1.0f); }

    // Framebuffer Image Views & Targets (Double-Buffered per Frame in Flight)
    VkImageView GetColorImageView(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].color.view; }
    VkImageView GetDepthImageView(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].depth.view; }
    VkImageView GetMSAAColorImageView(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].msaaColor.view; }
    VkImageView GetMSAADepthImageView(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].msaaDepth.view; }
    VkImage GetColorImage(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].color.image; }
    VkImage GetDepthImage(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].depth.image; }
    VkDescriptorSet GetDescriptorSet(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT].descriptorSet; }

    VkSampleCountFlagBits GetMSAASamples() const { return m_msaaSamples; }
    void SetMSAASamples(VkSampleCountFlagBits samples);

    VkSampler GetSampler() const { return m_sampler; }

    void TransitionToShaderRead(VkCommandBuffer cmd, uint32_t frame = 0);
    void TransitionToColorAttachment(VkCommandBuffer cmd, uint32_t frame = 0);

    void SetNativeWindow(GLFWwindow* window) { m_window = window; }
    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }
    void SetSelectNodeCallback(std::function<void(SceneNode*)> cb) { m_onSelectNode = std::move(cb); }
    [[nodiscard]] bool IsLightIconHovered() const noexcept { return m_hoveredLightNode != nullptr; }

    const RenderTargetFrame& GetFrame(uint32_t frame = 0) const { return m_frames[frame % MAX_FRAMES_IN_FLIGHT]; }

private:
    void DestroyFramebuffers();

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
    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT; // Fast default: 1x AA / Off (Peak Framerate & Low Latency)
    bool m_needTextureUpdate = false;
    bool m_isFocused = false;
    bool m_isHovered = false;
    bool m_cursorLocked = false;
    // Set true while the user is actively dragging a camera navigation button (LMB orbit,
    // RMB fly/look, MMB pan). Used to suppress gizmo/light hover highlights mid-drag.
    bool m_isCameraNavigating = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    // 3D Infinite Grid Settings
    bool m_showGrid = true;
    float m_gridCellSize = 1.0f;
    float m_gridMajorStep = 10.0f;
    float m_gridFadeDistance = 100.0f;
    float m_gridOpacity = 0.8f;

    // Double-buffered render target frames
    std::array<RenderTargetFrame, MAX_FRAMES_IN_FLIGHT> m_frames{};

    VkSampler m_sampler = VK_NULL_HANDLE;
    std::function<void(const std::string&)> m_onImportModel;
};

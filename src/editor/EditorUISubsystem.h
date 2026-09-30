#pragma once

#include "../core/ISubsystem.h"
#include "../core/EngineContext.h"
#include "ViewportPanel.h"
#include "SceneTreePanel.h"
#include "TimelinePanel.h"
#include "VulkanInspectorPanel.h"
#include "NodeGraphEditorPanel.h"
#include "AssetManagerPanel.h"
#include "ThumbnailCache.h"
#include "Theme.h"
#include "UIWidgets.h"
#include "VectorIcons.h"

#include <volk.h>
#include <imgui.h>
#include <memory>
#include <string>

namespace khepri {

/// Subsystem that owns the entire Dear ImGui lifecycle, docking workspace,
/// menu bar, keyboard shortcuts, and all editor UI panels.
///
/// Ensures deterministic teardown by releasing all panel smart pointers and
/// child resources before shutting down the ImGui Vulkan/GLFW backend.
class EditorUISubsystem : public ISubsystem {
public:
    EditorUISubsystem() = default;
    ~EditorUISubsystem() override;

    void Initialize(EngineContext& context) override;
    void Update(float deltaTime) override;
    void RenderUI() override;
    void Shutdown() override;

    [[nodiscard]] const char* GetName() const noexcept override {
        return "EditorUISubsystem";
    }

    // --- Frame Context & Panel Accessors ---
    void SetCurrentFrame(uint32_t frame, float deltaTime) noexcept {
        m_currentFrame = frame;
        m_deltaTime = deltaTime;
    }

    [[nodiscard]] ViewportPanel* GetViewportPanel() const noexcept {
        return m_viewportPanel.get();
    }
    [[nodiscard]] SceneTreePanel* GetSceneTreePanel() const noexcept {
        return m_sceneTreePanel.get();
    }
    [[nodiscard]] khepri::NodeGraphEditorPanel* GetNodeGraphEditorPanel() const noexcept {
        return m_nodeGraphEditorPanel.get();
    }
    [[nodiscard]] SceneNode* GetSelectedNode() const noexcept {
        return m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr;
    }
    [[nodiscard]] khepri::ui::ThumbnailCache* GetThumbnailCache() const noexcept {
        return m_thumbnailCache.get();
    }
    [[nodiscard]] float GetUIScale() const noexcept {
        return m_uiScale;
    }

    // --- Scene / Model Event Notifications ---
    void OnSceneLoaded(SceneNode* root, const std::shared_ptr<MeshComponent>& mesh);
    void OnModelImported(SceneNode* newlyAdded, const std::shared_ptr<MeshComponent>& mesh);
    void ClearSelection();
    void SetSelectedNode(SceneNode* node);

    // --- Layout & Menu ---
    void ApplyDockLayout(ImGuiID dockspaceID);
    void RenderMainMenuBar(ImGuiID dockspaceID);

private:
    void InitImGui();
    static void RenderEngineLogConsole(bool* p_open);

    EngineContext* m_ctx = nullptr;
    uint32_t m_currentFrame = 0;
    float m_deltaTime = 0.016f;
    bool m_initialized = false;

    // ImGui Vulkan descriptor pool
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;

    // Editor Panels (owned hierarchically by this subsystem)
    std::unique_ptr<ViewportPanel> m_viewportPanel;
    std::unique_ptr<SceneTreePanel> m_sceneTreePanel;
    std::unique_ptr<TimelinePanel> m_timelinePanel;
    std::unique_ptr<VulkanInspectorPanel> m_vulkanInspectorPanel;
    std::unique_ptr<khepri::NodeGraphEditorPanel> m_nodeGraphEditorPanel;
    std::unique_ptr<khepri::AssetManagerPanel> m_assetManagerPanel;
    std::unique_ptr<khepri::ui::ThumbnailCache> m_thumbnailCache;

    // UI & DPI Scale
    float m_uiScale = 1.0f;
    float m_pendingFontScale = 0.0f;

    // Window Visibility Toggles
    bool m_showViewport = true;
    bool m_showSceneTree = true;
    bool m_showNodeGraph = true;
    bool m_showTimeline = true;
    bool m_showAssetManager = true;
    bool m_showVulkanInspector = false;
    bool m_showLogConsole = true;

    bool m_rebuildLayout = true;
    char m_gltfPathInput[512] = "assets/models/Box.gltf";
    bool m_openGltfModal = false;
};

} // namespace khepri
#pragma once

#include "../core/Window.h"
#include "../core/Version.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Swapchain.h"
#include "../vulkan/Descriptors.h"
#include "../vulkan/Pipeline.h"
#include "../vulkan/Texture.h"
#include "../scene/Camera.h"
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../scene/Light.h"
#include "../vulkan/Buffer.h"
#include "../animation/Timeline.h"
#include "ViewportPanel.h"
#include "SceneTreePanel.h"
#include "TimelinePanel.h"
#include "VulkanInspectorPanel.h"
#include "NodeGraphEditorPanel.h"
#include "AssetManagerPanel.h"
#include "OracleBridge.h"
#include "../core/UndoStack.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <memory>
#include <array>
#include <unordered_map>
#include <vector>
#include <functional>

struct DeletionQueue {
    std::vector<std::function<void()>> deletors;

    void Push(std::function<void()>&& func) {
        deletors.emplace_back(std::move(func));
    }

    void Flush() {
        for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
            if (*it) {
                (*it)();
            }
        }
        deletors.clear();
    }
};

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    void Run();

    enum class EditorMode {
        MeshEditing,
        WorldBuilding
    };

    void SetEditorMode(EditorMode mode);
    EditorMode GetEditorMode() const { return m_currentMode; }

    void OpenSceneModel(const std::string& path);
    void ImportModelIntoScene(const std::string& path);
    void LoadGLTFModel(const std::string& path) { OpenSceneModel(path); }
    void LoadSampleModel(const std::string& name);

    [[nodiscard]] khepri::core::UndoStack& GetUndoStack() noexcept { return m_undoStack; }
    [[nodiscard]] const khepri::core::UndoStack& GetUndoStack() const noexcept { return m_undoStack; }

private:
    void InitImGui();
    void InitRenderResources();
    void CreateRenderPipeline();
    void CreateGridPipeline();
    void BuildSampleScene();
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame);
    void RenderViewportOffscreen(VkCommandBuffer cmd, uint32_t currentFrame);
    void RenderMainMenuBar(ImGuiID dockspaceID);
    void ApplyDockLayout(ImGuiID dockspaceID);

    void UpdateLightUBO(uint32_t currentFrame);
    void DrawSceneNode(VkCommandBuffer cmd, SceneNode* node, const glm::mat4& parentTransform, uint32_t currentFrame);

    Window m_window;
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    // Double-buffered Light UBOs and descriptor sets per frame in flight
    std::array<std::unique_ptr<Buffer>, Swapchain::MAX_FRAMES_IN_FLIGHT> m_lightUBOBuffers;
    VkDescriptorSetLayout m_lightDescriptorSetLayout = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, Swapchain::MAX_FRAMES_IN_FLIGHT> m_lightDescriptorSets{};

    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;

    // Viewport Mesh Pipeline
    VkDescriptorSetLayout m_textureDescriptorSetLayout = VK_NULL_HANDLE;
    std::shared_ptr<Texture> m_defaultWhiteTexture;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;

    // 3D Infinite Ground Grid Pipeline
    VkPipelineLayout m_gridPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_gridPipeline = VK_NULL_HANDLE;

    VkSampleCountFlagBits m_currentPipelineMSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // Command Buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Scene & Camera
    Camera m_camera;
    std::shared_ptr<SceneNode> m_rootNode;
    std::shared_ptr<MeshComponent> m_activeDisplayMesh; // tracked for inspector/camera focus
    Timeline m_timeline;
    khepri::core::UndoStack m_undoStack;

    // In-memory SPIR-V shader bytecode cache
    std::unordered_map<std::string, std::vector<uint32_t>> m_spirvCache;
    const std::vector<uint32_t>& GetOrLoadSPIRV(const std::string& path);

    // Editor UI Panels
    std::unique_ptr<ViewportPanel> m_viewportPanel;
    std::unique_ptr<SceneTreePanel> m_sceneTreePanel;
    std::unique_ptr<TimelinePanel> m_timelinePanel;
    std::unique_ptr<VulkanInspectorPanel> m_vulkanInspectorPanel;
    std::unique_ptr<khepri::NodeGraphEditorPanel> m_nodeGraphEditorPanel;
    std::unique_ptr<khepri::AssetManagerPanel> m_assetManagerPanel;

    // Window Visibility Toggles
    bool m_showViewport = true;
    bool m_showSceneTree = true;
    bool m_showNodeGraph = true;
    bool m_showTimeline = true;
    bool m_showAssetManager = true; // Open on load-in by default
    bool m_showVulkanInspector = false; // Untoggled by default
    bool m_showLogConsole = true;

    EditorMode m_currentMode = EditorMode::MeshEditing;
    bool m_rebuildLayout = true;
    char m_gltfPathInput[512] = "assets/models/Box.gltf";
    bool m_openGltfModal = false;
    khepri::OracleBridge m_oracleBridge;
    DeletionQueue m_deletionQueue;
};

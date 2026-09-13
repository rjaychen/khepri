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
#include "../core/EngineContext.h"
#include "../core/SubsystemManager.h"
#include "SceneRendererSubsystem.h"
#include "EditorUISubsystem.h"
#include "OracleBridge.h"
#include "../core/UndoStack.h"

#include <memory>
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

    [[nodiscard]] khepri::SubsystemManager& GetSubsystemManager() noexcept { return m_subsystemManager; }
    [[nodiscard]] khepri::SceneRendererSubsystem* GetSceneRenderer() const noexcept { return m_sceneRenderer; }
    [[nodiscard]] khepri::EditorUISubsystem* GetUISubsystem() const noexcept { return m_uiSubsystem; }

private:
    void BuildSampleScene();
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame);

    Window m_window;
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    // Command Buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Scene & Camera
    Camera m_camera;
    std::shared_ptr<SceneNode> m_rootNode;
    std::shared_ptr<MeshComponent> m_activeDisplayMesh;
    Timeline m_timeline;
    khepri::core::UndoStack m_undoStack;

    // Subsystem Management & Hierarchical Architecture
    khepri::SubsystemManager m_subsystemManager;
    std::unique_ptr<khepri::EngineContext> m_engineContext;
    khepri::SceneRendererSubsystem* m_sceneRenderer = nullptr;
    khepri::EditorUISubsystem* m_uiSubsystem = nullptr;

    EditorMode m_currentMode = EditorMode::MeshEditing;
    khepri::OracleBridge m_oracleBridge;
    DeletionQueue m_deletionQueue;
};


#pragma once

#include <imgui.h>
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../vulkan/VulkanContext.h"
#include <memory>
#include <filesystem>
#include <functional>

// Forward declaration to avoid circular include with NodeGraphEditorPanel.h
namespace khepri { class NodeGraphEditorPanel; }

class SceneTreePanel {
public:
    SceneTreePanel(VulkanContext& context);

    // activeMesh is updated when a new primitive is added so EditorApp can focus camera on it.
    // nodeGraphPanel (optional) is used to render the Node Properties section in the Inspector.
    void RenderUI(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh,
                  const std::filesystem::path& selectedAssetPath = "",
                  khepri::NodeGraphEditorPanel* nodeGraphPanel = nullptr);

    SceneNode* GetSelectedNode() const { return m_selectedNode; }
    void ClearSelectedNode() { m_selectedNode = nullptr; }

    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }
    void SetOpenModelCallback(std::function<void(const std::string&)> cb) { m_onOpenModel = std::move(cb); }

private:
    void RenderNodeTree(SceneNode* node);
    void RenderInspector(SceneNode* node, const std::filesystem::path& selectedAssetPath,
                         khepri::NodeGraphEditorPanel* nodeGraphPanel);
    void RenderFileAssetInspector(const std::filesystem::path& assetPath);

    VulkanContext& m_context;
    SceneNode* m_selectedNode = nullptr;
    int m_addPrimitiveType = 0; // 0: Cube, 1: Sphere, 2: Cylinder, 3: Plane

    std::function<void(const std::string&)> m_onImportModel;
    std::function<void(const std::string&)> m_onOpenModel;
};


#pragma once

#include <imgui.h>
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../vulkan/VulkanContext.h"
#include "Theme.h"
#include "UIWidgets.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <functional>
#include <string>

// Forward declarations
namespace khepri { class NodeGraphEditorPanel; }
namespace khepri::core { class UndoStack; }

class SceneTreePanel {
public:
    SceneTreePanel(VulkanContext* context = nullptr);
    SceneTreePanel(VulkanContext& context);
    ~SceneTreePanel() = default;

    // activeMesh is updated when a new primitive is added so EditorApp can focus camera on it.
    // nodeGraphPanel (optional) is used to render the Node Properties section in the Inspector.
    void RenderUI(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh,
                  const std::filesystem::path& selectedAssetPath = "",
                  khepri::NodeGraphEditorPanel* nodeGraphPanel = nullptr);

    // Single / Primary Selection API
    SceneNode* GetSelectedNode() const { return m_selectedNode; }
    void SetSelectedNode(SceneNode* node);
    void ClearSelectedNode();

    // Multi-Selection API
    const std::unordered_set<SceneNode*>& GetSelectedNodes() const { return m_selectedNodes; }
    bool IsNodeSelected(const SceneNode* node) const {
        return node && m_selectedNodes.find(const_cast<SceneNode*>(node)) != m_selectedNodes.end();
    }
    void SelectNode(SceneNode* node, bool additive = false);
    void DeselectNode(SceneNode* node);
    void SelectAll(SceneNode* rootNode);
    void ClearAllSelections();

    void ValidateSelection(const SceneNode* rootNode) noexcept;

    // Callbacks
    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }
    void SetOpenModelCallback(std::function<void(const std::string&)> cb) { m_onOpenModel = std::move(cb); }
    void SetFocusCameraCallback(std::function<void(const glm::vec3&)> cb) { m_onFocusCamera = std::move(cb); }

    void SetUndoStack(khepri::core::UndoStack* undoStack) noexcept { m_undoStack = undoStack; }
    [[nodiscard]] khepri::core::UndoStack* GetUndoStack() const noexcept { return m_undoStack; }

    // Inline Renaming trigger
    void StartRenaming(SceneNode* node);

    // Search filter
    const char* GetFilterText() const { return m_searchFilter; }
    void SetFilterText(const std::string& filter);

private:
    void RenderHeaderToolbar(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh);
    void RenderNodeTree(SceneNode* node, SceneNode* rootNode, int depth = 0, bool isLastChild = false, ImVec2 parentPos = ImVec2(0, 0));
    void RenderInspector(SceneNode* node, const std::filesystem::path& selectedAssetPath,
                         khepri::NodeGraphEditorPanel* nodeGraphPanel);
    void RenderFileAssetInspector(const std::filesystem::path& assetPath);

    void RenderNodeContextMenu(SceneNode* node, SceneNode* rootNode);
    void HandleMultiSelectClick(SceneNode* node, SceneNode* rootNode);
    void CollectFlattenedNodes(SceneNode* node, std::vector<SceneNode*>& outNodes);

    bool NodeMatchesFilter(const SceneNode* node, const std::string& filter) const;
    bool SubtreeMatchesFilter(const SceneNode* node, const std::string& filter) const;

    VulkanContext* m_context{nullptr};
    SceneNode* m_selectedNode{nullptr};
    std::unordered_set<SceneNode*> m_selectedNodes;
    SceneNode* m_lastSelectedNode{nullptr}; // For shift-range selection

    khepri::core::UndoStack* m_undoStack{nullptr};

    // Filter / Search State
    char m_searchFilter[128] = "";

    // Inline Renaming State
    SceneNode* m_renamingNode{nullptr};
    char m_renameBuffer[256] = "";
    bool m_focusRenameInput{false};

    // Transform drag state tracking for UndoStack
    glm::vec3 m_dragStartPos{0.0f};
    glm::vec3 m_dragStartRot{0.0f};
    glm::vec3 m_dragStartScale{1.0f};

    // Callbacks
    std::function<void(const std::string&)> m_onImportModel;
    std::function<void(const std::string&)> m_onOpenModel;
    std::function<void(const glm::vec3&)> m_onFocusCamera;
};

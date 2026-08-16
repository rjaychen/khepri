#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../graph/NodeGraph.h"
#include "../vulkan/VulkanContext.h"
#include "../scene/MeshComponent.h"
#include <imgui.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

class SceneNode;

namespace khepri {

class NodeGraphEditorPanel {
public:
    explicit NodeGraphEditorPanel(VulkanContext& context);
    ~NodeGraphEditorPanel() noexcept = default;

    NodeGraphEditorPanel(const NodeGraphEditorPanel&) = delete;
    NodeGraphEditorPanel& operator=(const NodeGraphEditorPanel&) = delete;

    void RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh);
    void SetImportedMesh(std::shared_ptr<MeshComponent> importedMesh);

    // Target SceneNode binding
    void SetTargetSceneNode(SceneNode* targetNode);
    [[nodiscard]] SceneNode* GetTargetSceneNode() const noexcept { return m_targetSceneNode; }

    // Returns the current active output mesh produced by the graph (for viewport display)
    [[nodiscard]] std::shared_ptr<MeshComponent> GetActiveOutputMesh() const noexcept;

    [[nodiscard]] std::shared_ptr<graph::NodeGraph> GetGraph() const noexcept { return m_graph; }
    [[nodiscard]] uint32_t GetSelectedNodeId() const noexcept { return m_selectedNodeId; }

    // Multi-selection management
    [[nodiscard]] const std::unordered_set<uint32_t>& GetSelectedNodeIds() const noexcept { return m_selectedNodeIds; }
    [[nodiscard]] bool IsNodeSelected(uint32_t id) const noexcept { return m_selectedNodeIds.count(id) > 0; }
    void SelectNode(uint32_t id, bool addToSelection = false);
    void DeselectNode(uint32_t id);
    void ClearSelection();

    // Display Flag (Preview specific intermediate node in viewport)
    [[nodiscard]] uint32_t GetPreviewNodeId() const noexcept { return m_previewNodeId; }
    void SetPreviewNodeId(uint32_t nodeId) noexcept { m_previewNodeId = nodeId; }

    // Renders editable node parameter controls into the Inspector panel
    void RenderNodePropertiesInspector();

    // Visual Node Editor UX helpers (exposed for UI testing & rendering)
    [[nodiscard]] static ImU32 GetPinColor(graph::PinType type) noexcept;
    [[nodiscard]] static ImU32 GetDomainHeaderColor(graph::NodeDomain domain) noexcept;
    [[nodiscard]] static float CalculateTangentLength(ImVec2 p0, ImVec2 p3) noexcept;

    [[nodiscard]] ImVec2 CanvasToScreenSpace(ImVec2 canvasPos, ImVec2 canvasOrigin) const noexcept;
    [[nodiscard]] ImVec2 ScreenToCanvasSpace(ImVec2 screenPos, ImVec2 canvasOrigin) const noexcept;

    [[nodiscard]] ImVec2 GetNodePosition(uint32_t nodeId) const noexcept;
    void SetNodePosition(uint32_t nodeId, ImVec2 pos) noexcept;

    [[nodiscard]] ImVec2 GetPanOffset() const noexcept { return m_panOffset; }
    void SetPanOffset(ImVec2 offset) noexcept { m_panOffset = offset; }
    [[nodiscard]] float GetZoom() const noexcept { return m_zoom; }
    void SetZoom(float zoom) noexcept { m_zoom = std::clamp(zoom, 0.25f, 2.0f); }

    // Auto-wiring helper when dropping wire to spawn node
    bool AutoConnectNodePin(uint32_t sourcePinId, uint32_t newNodeId);

private:
    void RenderToolbar();
    void RenderNodeCanvas();

    VulkanContext& m_context;
    SceneNode* m_targetSceneNode{nullptr};
    std::shared_ptr<graph::NodeGraph> m_graph;
    
    // Selection state
    uint32_t m_selectedNodeId{0};
    std::unordered_set<uint32_t> m_selectedNodeIds;
    uint32_t m_previewNodeId{0};

    // Canvas Navigation & Drag State
    ImVec2 m_panOffset{0.0f, 0.0f};
    float m_zoom{1.0f};
    std::unordered_map<uint32_t, ImVec2> m_nodePositions;
    std::unordered_map<uint32_t, ImVec2> m_pinScreenPositions;

    // Marquee (Box) Multi-Selection
    bool m_isBoxSelecting{false};
    ImVec2 m_boxSelectStart{0.0f, 0.0f};
    ImVec2 m_boxSelectEnd{0.0f, 0.0f};

    // Interactive Wiring Drag State
    bool m_isDraggingLink{false};
    uint32_t m_dragStartPinId{0};
    bool m_dragStartIsOutput{true};

    // Drag-to-empty-space drop location for node spawning
    bool m_openPinDropPopup{false};
    uint32_t m_droppedPinId{0};
    ImVec2 m_droppedCanvasPos{0.0f, 0.0f};
};

} // namespace khepri


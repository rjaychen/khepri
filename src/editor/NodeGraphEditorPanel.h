#pragma once

#include "../graph/NodeGraph.h"
#include "../vulkan/VulkanContext.h"
#include "../scene/MeshComponent.h"
#include <imgui.h>
#include <memory>

namespace khepri {

class NodeGraphEditorPanel {
public:
    explicit NodeGraphEditorPanel(VulkanContext& context);
    ~NodeGraphEditorPanel() noexcept = default;

    NodeGraphEditorPanel(const NodeGraphEditorPanel&) = delete;
    NodeGraphEditorPanel& operator=(const NodeGraphEditorPanel&) = delete;

    void RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh);
    void SetImportedMesh(std::shared_ptr<MeshComponent> importedMesh);

    [[nodiscard]] std::shared_ptr<graph::NodeGraph> GetGraph() const noexcept { return m_graph; }

private:
    void RenderToolbar();
    void RenderNodeCanvas();
    void RenderNodeProperties();

    VulkanContext& m_context;
    std::shared_ptr<graph::NodeGraph> m_graph;
    uint32_t m_selectedNodeId{0};
    int m_primitiveTypeCombo{0};
    int m_deformerTypeCombo{0};
    float m_twistAngle{45.0f};
    float m_waveAmplitude{0.2f};
};

} // namespace khepri

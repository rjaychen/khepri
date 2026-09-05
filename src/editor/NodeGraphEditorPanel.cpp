#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "NodeGraphEditorPanel.h"
#include "../graph/GeometryNodes.h"
#include "../scene/SceneNode.h"
#include "../core/Logger.h"
#include <imgui_internal.h>
#include <algorithm>

namespace khepri {

using namespace graph;

ImU32 NodeGraphEditorPanel::GetPinColor(graph::PinType type) noexcept {
    switch (type) {
    case graph::PinType::GeometryBuffer: return IM_COL32(0, 229, 255, 255);  // Teal #00E5FF
    case graph::PinType::Material:       return IM_COL32(255, 215, 0, 255);  // Gold #FFD700
    case graph::PinType::Float:          return IM_COL32(220, 220, 220, 255); // Silver #E0E0E0
    case graph::PinType::Vector3:        return IM_COL32(255, 111, 97, 255);  // Coral #FF6F61
    case graph::PinType::Matrix4:        return IM_COL32(255, 140, 0, 255);  // Orange #FF8C00
    }
    return IM_COL32(255, 255, 255, 255);
}

ImU32 NodeGraphEditorPanel::GetDomainHeaderColor(graph::NodeDomain domain) noexcept {
    switch (domain) {
    case graph::NodeDomain::Geometry:  return IM_COL32(27, 79, 114, 255);  // Deep Blue/Teal #1B4F72
    case graph::NodeDomain::Material:  return IM_COL32(30, 132, 73, 255);  // Emerald Green #1E8449
    case graph::NodeDomain::Animation: return IM_COL32(108, 52, 131, 255); // Amethyst Purple #6C3483
    }
    return IM_COL32(60, 60, 70, 255);
}

float NodeGraphEditorPanel::CalculateTangentLength(ImVec2 p0, ImVec2 p3) noexcept {
    const float dx = std::abs(p3.x - p0.x);
    const float halfDx = dx * 0.5f;
    return (50.0f > halfDx) ? 50.0f : halfDx;
}

ImVec2 NodeGraphEditorPanel::CanvasToScreenSpace(ImVec2 canvasPos, ImVec2 canvasOrigin) const noexcept {
    return ImVec2(
        canvasOrigin.x + m_panOffset.x + canvasPos.x * m_zoom,
        canvasOrigin.y + m_panOffset.y + canvasPos.y * m_zoom
    );
}

ImVec2 NodeGraphEditorPanel::ScreenToCanvasSpace(ImVec2 screenPos, ImVec2 canvasOrigin) const noexcept {
    return ImVec2(
        (screenPos.x - canvasOrigin.x - m_panOffset.x) / m_zoom,
        (screenPos.y - canvasOrigin.y - m_panOffset.y) / m_zoom
    );
}

ImVec2 NodeGraphEditorPanel::GetNodePosition(uint32_t nodeId) const noexcept {
    auto it = m_nodePositions.find(nodeId);
    if (it != m_nodePositions.end()) {
        return it->second;
    }
    return ImVec2(50.0f + static_cast<float>((nodeId - 1) % 4) * 270.0f, 80.0f);
}

void NodeGraphEditorPanel::SetNodePosition(uint32_t nodeId, ImVec2 pos) noexcept {
    m_nodePositions[nodeId] = pos;
}

void NodeGraphEditorPanel::SelectNode(uint32_t id, bool addToSelection) {
    if (!addToSelection) {
        m_selectedNodeIds.clear();
    }
    m_selectedNodeIds.insert(id);
    m_selectedNodeId = id;
}

void NodeGraphEditorPanel::DeselectNode(uint32_t id) {
    m_selectedNodeIds.erase(id);
    if (m_selectedNodeId == id) {
        m_selectedNodeId = m_selectedNodeIds.empty() ? 0 : *m_selectedNodeIds.begin();
    }
}

void NodeGraphEditorPanel::ClearSelection() {
    m_selectedNodeIds.clear();
    m_selectedNodeId = 0;
}

void NodeGraphEditorPanel::SetTargetSceneNode(SceneNode* targetNode) {
    m_targetSceneNode = targetNode;
    ClearSelection();
    m_previewNodeId = 0;

    if (m_targetSceneNode) {
        m_graph = m_targetSceneNode->GetOrCreateNodeGraph(&m_context);
    } else if (!m_graph) {
        m_graph = std::make_shared<NodeGraph>();
    }
}

bool NodeGraphEditorPanel::AutoConnectNodePin(uint32_t sourcePinId, uint32_t newNodeId) {
    if (!m_graph) return false;
    const GraphPin* srcPin = m_graph->FindPin(sourcePinId);
    auto newNode = m_graph->GetNode(newNodeId);
    if (!srcPin || !newNode) return false;

    if (srcPin->direction == PinDirection::Output) {
        for (const auto& inPin : newNode->GetInputs()) {
            if (inPin.type == srcPin->type) {
                if (m_graph->Connect(srcPin->id, inPin.id)) {
                    m_graph->Evaluate();
                    if (m_targetSceneNode) {
                        m_targetSceneNode->mesh = GetActiveOutputMesh();
                    }
                    return true;
                }
            }
        }
    } else {
        for (const auto& outPin : newNode->GetOutputs()) {
            if (outPin.type == srcPin->type) {
                if (m_graph->Connect(outPin.id, srcPin->id)) {
                    m_graph->Evaluate();
                    if (m_targetSceneNode) {
                        m_targetSceneNode->mesh = GetActiveOutputMesh();
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

NodeGraphEditorPanel::NodeGraphEditorPanel(VulkanContext& context)
    : m_context(context) {
    m_graph = std::make_shared<NodeGraph>();

    auto primitiveNode = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cube);
    auto subdivNode    = m_graph->CreateNode<SubdivisionNode>(&m_context, 2);
    auto twistNode     = m_graph->CreateNode<TwistDeformerNode>(&m_context);

    // Connect Primitive -> Subdivision -> Twist Deformer
    if (primitiveNode->FindOutput("MeshBuffer") && subdivNode->FindInput("MeshBuffer")) {
        (void)m_graph->Connect(primitiveNode->FindOutput("MeshBuffer")->id, subdivNode->FindInput("MeshBuffer")->id);
    }
    if (subdivNode->FindOutput("SubdividedMeshBuffer") && twistNode->FindInput("MeshBuffer")) {
        (void)m_graph->Connect(subdivNode->FindOutput("SubdividedMeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id);
    }

    m_nodePositions[primitiveNode->GetId()] = ImVec2(50.0f, 80.0f);
    m_nodePositions[subdivNode->GetId()]    = ImVec2(320.0f, 80.0f);
    m_nodePositions[twistNode->GetId()]     = ImVec2(590.0f, 80.0f);

    m_graph->Evaluate();
    SelectNode(twistNode->GetId());
}

std::shared_ptr<MeshComponent> NodeGraphEditorPanel::GetActiveOutputMesh() const noexcept {
    if (!m_graph) return nullptr;

    // 1. If explicit preview node is set (via Display Flag / Eye icon), return its mesh
    if (m_previewNodeId != 0) {
        if (auto prevNode = m_graph->GetNode(m_previewNodeId)) {
            if (auto prevMesh = prevNode->GetOutputMesh()) {
                return prevMesh;
            }
        }
    }

    // 2. Active single selection preview: if selected node produces an output mesh, show it
    if (m_selectedNodeId != 0 && m_previewNodeId == 0) {
        if (auto selNode = m_graph->GetNode(m_selectedNodeId)) {
            if (auto selMesh = selNode->GetOutputMesh()) {
                return selMesh;
            }
        }
    }

    // 3. Fallback to topological terminal leaf node (the latest geometry node in DAG)
    auto sortedNodes = m_graph->TopologicalSort();
    for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
        if (*it) {
            if (auto mesh = (*it)->GetOutputMesh()) {
                return mesh;
            }
        }
    }

    return nullptr;
}

void NodeGraphEditorPanel::RenderNodePropertiesInspector() {
    if (m_selectedNodeId == 0 || !m_graph) {
        ImGui::TextDisabled("No graph node selected.");
        return;
    }
    auto node = m_graph->GetNode(m_selectedNodeId);
    if (!node) {
        ImGui::TextDisabled("Selected node not found.");
        return;
    }

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[%u] %s", m_selectedNodeId, node->GetName().c_str());
    ImGui::Separator();

    bool changed = false;

    if (auto primNode = std::dynamic_pointer_cast<MeshPrimitiveNode>(node)) {
        int currentType = static_cast<int>(primNode->GetPrimitiveType());
        const char* primTypes[] = { "Cube", "Sphere", "Cylinder", "Plane" };
        ImGui::Text("Primitive Type");
        if (ImGui::Combo("##InspPrimType", &currentType, primTypes, IM_ARRAYSIZE(primTypes))) {
            primNode->SetPrimitiveType(static_cast<MeshPrimitiveNode::PrimitiveType>(currentType));
            changed = true;
        }
    }
    if (auto subdivNode = std::dynamic_pointer_cast<SubdivisionNode>(node)) {
        int level = static_cast<int>(subdivNode->GetSubdivisionLevel());
        ImGui::Text("Subdivision Level");
        if (ImGui::SliderInt("##InspLevel", &level, 0, 5)) {
            subdivNode->SetSubdivisionLevel(static_cast<uint32_t>(level));
            auto* lvlPin = subdivNode->FindInput("Level");
            if (lvlPin) lvlPin->value = static_cast<float>(level);
            changed = true;
        }
        bool applyChildren = subdivNode->GetApplyToChildren();
        if (ImGui::Checkbox("Apply to Child Objects", &applyChildren)) {
            subdivNode->SetApplyToChildren(applyChildren);
            changed = true;
        }
    }
    if (auto twistNode = std::dynamic_pointer_cast<TwistDeformerNode>(node)) {
        float angle = twistNode->GetAngle();
        ImGui::Text("Twist Angle");
        if (ImGui::SliderFloat("##InspAngle", &angle, -360.0f, 360.0f, "%.1f deg")) {
            twistNode->SetAngle(angle);
            auto* anglePin = twistNode->FindInput("Angle");
            if (anglePin) anglePin->value = angle;
            changed = true;
        }
        bool applyChildren = twistNode->GetApplyToChildren();
        if (ImGui::Checkbox("Apply to Child Objects", &applyChildren)) {
            twistNode->SetApplyToChildren(applyChildren);
            changed = true;
        }
    }

    if (auto floatNode = std::dynamic_pointer_cast<FloatNode>(node)) {
        float val = floatNode->GetValue();
        ImGui::Text("Float Value");
        if (ImGui::DragFloat("##InspFloat", &val, 0.05f, -1000.0f, 1000.0f, "%.3f")) {
            floatNode->SetValue(val);
            changed = true;
        }
    }
    if (auto vec3Node = std::dynamic_pointer_cast<Vector3Node>(node)) {
        glm::vec3 vec = vec3Node->GetValue();
        ImGui::Text("Vector3 Value");
        if (ImGui::DragFloat3("##InspVec3", &vec.x, 0.05f, -1000.0f, 1000.0f, "%.2f")) {
            vec3Node->SetValue(vec);
            changed = true;
        }
    }

    // Show all connected input pins
    ImGui::Spacing();
    ImGui::TextDisabled("Input Pins");
    ImGui::Separator();
    for (auto& inPin : node->GetInputs()) {
        if (inPin.connectedPinId != 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "  %s: (Wired)", inPin.name.c_str());
        }
    }

    if (changed) {
        m_graph->Evaluate();
        if (m_targetSceneNode) {
            m_targetSceneNode->mesh = GetActiveOutputMesh();
        }
    }
}

void NodeGraphEditorPanel::SetImportedMesh(std::shared_ptr<MeshComponent> importedMesh) {
    if (!importedMesh) return;
    if (!m_graph) m_graph = std::make_shared<NodeGraph>();

    std::shared_ptr<graph::ExternalMeshNode> extNode = nullptr;
    for (const auto& [id, node] : m_graph->GetNodes()) {
        if (auto ext = std::dynamic_pointer_cast<graph::ExternalMeshNode>(node)) {
            extNode = ext;
            break;
        }
    }

    if (!extNode) {
        extNode = m_graph->CreateNode<graph::ExternalMeshNode>(importedMesh);
        m_nodePositions[extNode->GetId()] = ImVec2(50.0f, 80.0f);

        // Connect to first available deformer if present
        for (const auto& [id, node] : m_graph->GetNodes()) {
            if (auto subNode = std::dynamic_pointer_cast<graph::SubdivisionNode>(node)) {
                (void)m_graph->Connect(extNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);
                break;
            } else if (auto twistNode = std::dynamic_pointer_cast<graph::TwistDeformerNode>(node)) {
                (void)m_graph->Connect(extNode->FindOutput("MeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id);
                break;
            }
        }
    } else {
        extNode->SetExternalMesh(importedMesh);
    }

    SelectNode(extNode->GetId());
    m_graph->Evaluate();
    if (m_targetSceneNode) {
        m_targetSceneNode->mesh = GetActiveOutputMesh();
    }
}

void NodeGraphEditorPanel::RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh) {
    ImGui::Begin("Procedural Node Graph Editor");

    RenderToolbar();
    ImGui::Separator();

    RenderNodeCanvas();

    // Push active graph result to display mesh
    auto graphMesh = GetActiveOutputMesh();
    if (graphMesh) {
        activeDisplayMesh = graphMesh;
        if (m_targetSceneNode) {
            m_targetSceneNode->mesh = graphMesh;
        }
    }

    ImGui::End();
}

void NodeGraphEditorPanel::RenderToolbar() {
    // Target SceneNode Breadcrumb Indicator
    if (m_targetSceneNode) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Target Object: [%s]", m_targetSceneNode->name.c_str());
    } else {
        ImGui::TextDisabled("Target Object: [None (Default Scene Graph)]");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (ImGui::Button("+ Add Node...")) {
        ImGui::OpenPopup("ToolbarAddNodeMenu");
    }

    if (ImGui::BeginPopup("ToolbarAddNodeMenu")) {
        if (!m_graph) m_graph = std::make_shared<NodeGraph>();
        const ImVec2 spawnPos(100.0f - m_panOffset.x, 100.0f - m_panOffset.y);

        if (ImGui::BeginMenu("Primitives")) {
            if (ImGui::MenuItem("Cube")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cube);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("Sphere")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Sphere);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("Cylinder")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cylinder);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("Plane")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Plane);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Deformers")) {
            if (ImGui::MenuItem("Subdivision Node")) {
                auto node = m_graph->CreateNode<SubdivisionNode>(&m_context, 2);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("Twist Deformer Node")) {
                auto node = m_graph->CreateNode<TwistDeformerNode>(&m_context);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Value Nodes")) {
            if (ImGui::MenuItem("Float Value Node")) {
                auto node = m_graph->CreateNode<FloatNode>(1.0f);
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("Vector3 Value Node")) {
                auto node = m_graph->CreateNode<Vector3Node>(glm::vec3(0.0f));
                m_nodePositions[node->GetId()] = spawnPos;
                SelectNode(node->GetId());
                m_graph->Evaluate();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Imported Mesh Node")) {
            auto node = m_graph->CreateNode<graph::ExternalMeshNode>(nullptr);
            m_nodePositions[node->GetId()] = spawnPos;
            SelectNode(node->GetId());
            m_graph->Evaluate();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_panOffset = ImVec2(0.0f, 0.0f);
        m_zoom = 1.0f;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Zoom: %.0f%%", m_zoom * 100.0f);

    ImGui::SameLine();
    if (ImGui::Button("Evaluate Graph")) {
        if (m_graph) {
            m_graph->Evaluate();
            if (m_targetSceneNode) {
                m_targetSceneNode->mesh = GetActiveOutputMesh();
            }
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox("Verbose Logs", &graph::GraphNode::s_enableGraphLogging);

    if (m_previewNodeId != 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Previewing Node [%u]", m_previewNodeId);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Preview")) {
            m_previewNodeId = 0;
        }
    }
}

void NodeGraphEditorPanel::RenderNodeCanvas() {
    if (!m_graph) return;

    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    if (canvasSize.x < 10.0f || canvasSize.y < 10.0f) return;

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    const bool isWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootAndChildWindows);

    // Canvas Panning: Middle-mouse drag OR Right-mouse drag on canvas
    if (isWindowHovered && !m_isDraggingLink && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) && !ImGui::IsPopupOpen("CanvasContextMenu")))) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_panOffset.x += delta.x;
        m_panOffset.y += delta.y;
    }

    // Canvas Zooming (Mouse Wheel)
    if (isWindowHovered && ImGui::GetIO().MouseWheel != 0.0f) {
        const float zoomFactor = ImGui::GetIO().MouseWheel > 0.0f ? 1.1f : 0.9f;
        const ImVec2 mouseCanvasBefore = ScreenToCanvasSpace(mousePos, canvasOrigin);
        m_zoom = std::clamp(m_zoom * zoomFactor, 0.25f, 2.0f);
        const ImVec2 mouseCanvasAfter = CanvasToScreenSpace(mouseCanvasBefore, canvasOrigin);
        m_panOffset.x += (mousePos.x - mouseCanvasAfter.x);
        m_panOffset.y += (mousePos.y - mouseCanvasAfter.y);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y), true);

    // 1. Canvas Background Grid
    drawList->AddRectFilled(canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y), IM_COL32(24, 24, 28, 255));

    const float gridStep = 48.0f * m_zoom;
    const float gridOffsetX = fmodf(m_panOffset.x, gridStep);
    const float gridOffsetY = fmodf(m_panOffset.y, gridStep);

    for (float x = gridOffsetX; x < canvasSize.x; x += gridStep) {
        drawList->AddLine(ImVec2(canvasOrigin.x + x, canvasOrigin.y),
                          ImVec2(canvasOrigin.x + x, canvasOrigin.y + canvasSize.y),
                          IM_COL32(38, 38, 44, 255), 1.0f);
    }
    for (float y = gridOffsetY; y < canvasSize.y; y += gridStep) {
        drawList->AddLine(ImVec2(canvasOrigin.x, canvasOrigin.y + y),
                          ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + y),
                          IM_COL32(38, 38, 44, 255), 1.0f);
    }

    // 2. Draw Existing Spline Links
    for (const auto& [nodeId, node] : m_graph->GetNodes()) {
        (void)nodeId;
        for (const auto& inPin : node->GetInputs()) {
            if (inPin.connectedPinId != 0) {
                auto p0It = m_pinScreenPositions.find(inPin.connectedPinId);
                auto p3It = m_pinScreenPositions.find(inPin.id);
                if (p0It != m_pinScreenPositions.end() && p3It != m_pinScreenPositions.end()) {
                    const ImVec2 p0 = p0It->second;
                    const ImVec2 p3 = p3It->second;
                    const float tangentLen = CalculateTangentLength(p0, p3);
                    const ImVec2 p1(p0.x + tangentLen, p0.y);
                    const ImVec2 p2(p3.x - tangentLen, p3.y);

                    const GraphPin* outPin = m_graph->FindPin(inPin.connectedPinId);
                    const ImU32 linkColor = outPin ? GetPinColor(outPin->type) : IM_COL32(200, 200, 200, 255);

                    drawList->AddBezierCubic(p0, p1, p2, p3, linkColor, 3.2f * m_zoom, 32);
                }
            }
        }
    }

    // 3. Draw Active Wire Dragging Spline
    uint32_t hoveredTargetPinId = 0;

    if (m_isDraggingLink) {
        auto startPinIt = m_pinScreenPositions.find(m_dragStartPinId);
        if (startPinIt != m_pinScreenPositions.end()) {
            const ImVec2 pStart = startPinIt->second;
            const ImVec2 pEnd = mousePos;
            const float tangentLen = CalculateTangentLength(pStart, pEnd);

            ImVec2 p1, p2;
            if (m_dragStartIsOutput) {
                p1 = ImVec2(pStart.x + tangentLen, pStart.y);
                p2 = ImVec2(pEnd.x - tangentLen, pEnd.y);
            } else {
                p1 = ImVec2(pStart.x - tangentLen, pStart.y);
                p2 = ImVec2(pEnd.x + tangentLen, pEnd.y);
            }

            const GraphPin* startPin = m_graph->FindPin(m_dragStartPinId);
            const ImU32 wireColor = startPin ? GetPinColor(startPin->type) : IM_COL32(0, 229, 255, 255);
            drawList->AddBezierCubic(pStart, p1, p2, pEnd, wireColor, 3.5f * m_zoom, 32);
        }
    }

    // 4. Render Node Cards, Inline Widgets & Pin Badges
    const float headerHeight = 28.0f * m_zoom;
    const float pinRadius = 6.0f * m_zoom;
    const float pinSpacing = 28.0f * m_zoom;
    bool anyCardHovered = false;

    for (const auto& nodePair : m_graph->GetNodes()) {
        const uint32_t id = nodePair.first;
        const auto& node = nodePair.second;
        const ImVec2 canvasPos = GetNodePosition(id);
        const ImVec2 nodeScreenPos = CanvasToScreenSpace(canvasPos, canvasOrigin);

        const size_t inCount = node->GetInputs().size();
        const size_t outCount = node->GetOutputs().size();
        const size_t maxPinRows = (inCount > outCount) ? inCount : outCount;
        
        float extraHeight = 0.0f;
        if (std::dynamic_pointer_cast<MeshPrimitiveNode>(node)) {
            extraHeight = 36.0f * m_zoom;


        float nodeWidth = 240.0f * m_zoom;
        if (std::dynamic_pointer_cast<Vector3Node>(node)) {
            nodeWidth = 280.0f * m_zoom;
        }

        const float pinContentHeight = static_cast<float>(maxPinRows) * pinSpacing + 14.0f * m_zoom;
        const float minContentHeight = (36.0f * m_zoom > pinContentHeight) ? (36.0f * m_zoom) : pinContentHeight;
        const float nodeBodyHeight = headerHeight + minContentHeight + extraHeight;

        const ImVec2 nodeMin = nodeScreenPos;
        const ImVec2 nodeMax = ImVec2(nodeScreenPos.x + nodeWidth, nodeScreenPos.y + nodeBodyHeight);

        const bool isSelected = IsNodeSelected(id);
        const bool isPreview = (m_previewNodeId == id);

        if (mousePos.x >= nodeMin.x && mousePos.x <= nodeMax.x && mousePos.y >= nodeMin.y && mousePos.y <= nodeMax.y) {
            anyCardHovered = true;
        }

        // Card Shadow & Body Background
        drawList->AddRectFilled(ImVec2(nodeMin.x + 4.0f, nodeMin.y + 4.0f), ImVec2(nodeMax.x + 4.0f, nodeMax.y + 4.0f), IM_COL32(0, 0, 0, 100), 8.0f);
        drawList->AddRectFilled(nodeMin, nodeMax, IM_COL32(32, 32, 38, 245), 8.0f);

        // Header Background
        const ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);
        const ImU32 headerColor = GetDomainHeaderColor(node->GetDomain());
        drawList->AddRectFilled(nodeMin, headerMax, headerColor, 8.0f, ImDrawFlags_RoundCornersTop);

        // Header Title
        const std::string headerTitle = "[" + std::to_string(id) + "] " + node->GetName();
        drawList->AddText(ImVec2(nodeMin.x + 8.0f * m_zoom, nodeMin.y + 6.0f * m_zoom), IM_COL32(240, 240, 240, 255), headerTitle.c_str());

        // Display Flag / Preview Eye Icon on right of header
        const float previewBtnWidth = 24.0f * m_zoom;
        const ImVec2 prevBtnPos(nodeMax.x - previewBtnWidth - 4.0f * m_zoom, nodeMin.y + 3.0f * m_zoom);
        const ImU32 eyeColor = isPreview ? IM_COL32(255, 215, 0, 255) : IM_COL32(180, 180, 190, 200);
        drawList->AddText(prevBtnPos, eyeColor, isPreview ? "[*]" : "[ ]");

        // Card Border / Selection Highlight
        if (isSelected) {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(0, 229, 255, 255), 8.0f, 0, 2.5f);
        } else if (isPreview) {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 215, 0, 255), 8.0f, 0, 2.0f);
        } else {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(55, 55, 65, 255), 8.0f, 0, 1.2f);
        }

        // --- Header Drag Interaction (Constrained to Header Bar only) ---
        ImGui::SetCursorScreenPos(nodeMin);
        ImGui::PushID(static_cast<int>(id));
        ImGui::InvisibleButton("##HeaderDrag", ImVec2(nodeWidth, headerHeight));

        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Check if clicked the Preview button on header
                if (mousePos.x >= prevBtnPos.x - 4.0f * m_zoom) {
                    m_previewNodeId = (m_previewNodeId == id) ? 0 : id;
                    if (m_targetSceneNode) {
                        m_targetSceneNode->mesh = GetActiveOutputMesh();
                    }
                } else {
                    const bool shiftPressed = ImGui::GetIO().KeyShift;
                    if (shiftPressed) {
                        if (isSelected) DeselectNode(id);
                        else SelectNode(id, true);
                    } else {
                        if (!isSelected) SelectNode(id, false);
                    }
                }
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            const float dx = delta.x / m_zoom;
            const float dy = delta.y / m_zoom;

            if (!isSelected) {
                SelectNode(id, false);
            }

            // Translate all selected nodes synchronously
            for (uint32_t selId : m_selectedNodeIds) {
                m_nodePositions[selId] = GetNodePosition(selId);
                m_nodePositions[selId].x += dx;
                m_nodePositions[selId].y += dy;
            }
        }

        // --- Render Input Pins (Left side) with Inline Controls ---
        float currentPinY = nodeMin.y + headerHeight + 16.0f * m_zoom;
        for (auto& inPin : node->GetInputs()) {
            const ImVec2 pinCenter(nodeMin.x + 12.0f * m_zoom, currentPinY);
            m_pinScreenPositions[inPin.id] = pinCenter;

            const ImU32 pinColor = GetPinColor(inPin.type);
            const float distToMouse = sqrtf((mousePos.x - pinCenter.x) * (mousePos.x - pinCenter.x) + (mousePos.y - pinCenter.y) * (mousePos.y - pinCenter.y));
            const bool pinHovered = (distToMouse <= pinRadius * 1.8f);

            if (pinHovered) {
                drawList->AddCircleFilled(pinCenter, pinRadius * 1.5f, IM_COL32(255, 255, 255, 100));
                hoveredTargetPinId = inPin.id;
            }
            drawList->AddCircleFilled(pinCenter, pinRadius, pinColor);
            drawList->AddCircle(pinCenter, pinRadius, IM_COL32(20, 20, 20, 255), 0, 1.5f);

            if (pinHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_isDraggingLink = true;
                m_dragStartPinId = inPin.id;
                m_dragStartIsOutput = false;
            }

            const float labelX = pinCenter.x + 10.0f * m_zoom;
            const float labelY = pinCenter.y - 7.0f * m_zoom;

            if (inPin.connectedPinId != 0) {
                const std::string wiredText = inPin.name + ": (Wired)";
                drawList->AddText(ImVec2(labelX, labelY), IM_COL32(100, 220, 100, 255), wiredText.c_str());
            } else {
                drawList->AddText(ImVec2(labelX, labelY), IM_COL32(200, 200, 210, 255), inPin.name.c_str());

                if (inPin.type == PinType::Float) {
                    const ImVec2 textSize = ImGui::CalcTextSize(inPin.name.c_str());
                    const float widgetX = labelX + textSize.x + 6.0f * m_zoom;
                    const float widgetWidth = (std::clamp)(80.0f * m_zoom, 40.0f * m_zoom, nodeWidth - (widgetX - nodeMin.x) - 10.0f * m_zoom);

                    ImGui::SetCursorScreenPos(ImVec2(widgetX, currentPinY - 10.0f * m_zoom));
                    ImGui::SetNextItemWidth(widgetWidth);
                    ImGui::PushID(static_cast<int>(inPin.id));

                    if (inPin.name == "Level") {
                        auto subdivNode = std::dynamic_pointer_cast<SubdivisionNode>(node);
                        int level = subdivNode ? static_cast<int>(subdivNode->GetSubdivisionLevel()) :
                                    (std::holds_alternative<float>(inPin.value) ? static_cast<int>(std::get<float>(inPin.value)) : 0);
                        if (ImGui::DragInt("##PinLvl", &level, 1, 0, 5, "Lvl: %d")) {
                            inPin.value = static_cast<float>(level);
                            if (subdivNode) subdivNode->SetSubdivisionLevel(static_cast<uint32_t>(level));
                            m_graph->Evaluate();
                            if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                        }
                    } else if (inPin.name == "Angle") {
                        auto twistNode = std::dynamic_pointer_cast<TwistDeformerNode>(node);
                        float angle = twistNode ? twistNode->GetAngle() :
                                      (std::holds_alternative<float>(inPin.value) ? std::get<float>(inPin.value) : 0.0f);
                        if (ImGui::DragFloat("##PinAngle", &angle, 1.0f, -360.0f, 360.0f, "%.1f°")) {
                            inPin.value = angle;
                            if (twistNode) twistNode->SetAngle(angle);
                            m_graph->Evaluate();
                            if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                        }
                    } else {
                        float val = std::holds_alternative<float>(inPin.value) ? std::get<float>(inPin.value) : 0.0f;
                        if (ImGui::DragFloat("##PinFloat", &val, 0.05f, -1000.0f, 1000.0f, "%.2f")) {
                            inPin.value = val;
                            m_graph->Evaluate();
                            if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                        }
                    }
                    ImGui::PopID();
                } else if (inPin.type == PinType::Vector3) {
                    const ImVec2 textSize = ImGui::CalcTextSize(inPin.name.c_str());
                    const float widgetX = labelX + textSize.x + 6.0f * m_zoom;

                    ImGui::SetCursorScreenPos(ImVec2(widgetX, currentPinY - 10.0f * m_zoom));
                    ImGui::SetNextItemWidth(125.0f * m_zoom);
                    ImGui::PushID(static_cast<int>(inPin.id));
                    glm::vec3 vec = std::holds_alternative<glm::vec3>(inPin.value) ? std::get<glm::vec3>(inPin.value) : glm::vec3(0.0f);
                    if (ImGui::DragFloat3("##PinVec3", &vec.x, 0.05f, -1000.0f, 1000.0f, "%.1f")) {
                        inPin.value = vec;
                        m_graph->Evaluate();
                        if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                    }
                    ImGui::PopID();
                }
            }

            currentPinY += pinSpacing;
        }

        // --- Render Output Pins (Right side) with Value Generator Controls ---
        currentPinY = nodeMin.y + headerHeight + 16.0f * m_zoom;
        for (auto& outPin : node->GetOutputs()) {
            const ImVec2 pinCenter(nodeMax.x - 12.0f * m_zoom, currentPinY);
            m_pinScreenPositions[outPin.id] = pinCenter;

            const ImU32 pinColor = GetPinColor(outPin.type);
            const float distToMouse = sqrtf((mousePos.x - pinCenter.x) * (mousePos.x - pinCenter.x) + (mousePos.y - pinCenter.y) * (mousePos.y - pinCenter.y));
            const bool pinHovered = (distToMouse <= pinRadius * 1.8f);

            if (pinHovered) {
                drawList->AddCircleFilled(pinCenter, pinRadius * 1.5f, IM_COL32(255, 255, 255, 100));
                hoveredTargetPinId = outPin.id;
            }
            drawList->AddCircleFilled(pinCenter, pinRadius, pinColor);
            drawList->AddCircle(pinCenter, pinRadius, IM_COL32(20, 20, 20, 255), 0, 1.5f);

            const ImVec2 labelSize = ImGui::CalcTextSize(outPin.name.c_str());
            const float labelX = pinCenter.x - 10.0f * m_zoom - labelSize.x;
            const float labelY = pinCenter.y - 7.0f * m_zoom;
            drawList->AddText(ImVec2(labelX, labelY), IM_COL32(200, 200, 210, 255), outPin.name.c_str());

            if (pinHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_isDraggingLink = true;
                m_dragStartPinId = outPin.id;
                m_dragStartIsOutput = true;
            }

            if (auto floatNode = std::dynamic_pointer_cast<FloatNode>(node)) {
                float val = floatNode->GetValue();
                const float widgetWidth = 85.0f * m_zoom;
                ImGui::SetCursorScreenPos(ImVec2(labelX - widgetWidth - 6.0f * m_zoom, currentPinY - 10.0f * m_zoom));
                ImGui::SetNextItemWidth(widgetWidth);
                ImGui::PushID(static_cast<int>(outPin.id));
                if (ImGui::DragFloat("##OutFloatVal", &val, 0.05f, -1000.0f, 1000.0f, "%.2f")) {
                    floatNode->SetValue(val);
                    m_graph->Evaluate();
                    if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                }
                ImGui::PopID();
            } else if (auto vec3Node = std::dynamic_pointer_cast<Vector3Node>(node)) {
                glm::vec3 vec = vec3Node->GetValue();
                const float widgetWidth = 140.0f * m_zoom;
                ImGui::SetCursorScreenPos(ImVec2(labelX - widgetWidth - 6.0f * m_zoom, currentPinY - 10.0f * m_zoom));
                ImGui::SetNextItemWidth(widgetWidth);
                ImGui::PushID(static_cast<int>(outPin.id));
                if (ImGui::DragFloat3("##OutVec3Val", &vec.x, 0.05f, -1000.0f, 1000.0f, "%.1f")) {
                    vec3Node->SetValue(vec);
                    m_graph->Evaluate();
                    if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
                }
                ImGui::PopID();
            }

            currentPinY += pinSpacing;
        }

        // --- Render Extra Body Controls (Primitive Type combo, CSG Boolean Op) ---
        if (auto primNode = std::dynamic_pointer_cast<MeshPrimitiveNode>(node)) {
            ImGui::SetCursorScreenPos(ImVec2(nodeMin.x + 10.0f * m_zoom, nodeMax.y - extraHeight + 4.0f * m_zoom));
            ImGui::SetNextItemWidth(nodeWidth - 20.0f * m_zoom);
            int currentType = static_cast<int>(primNode->GetPrimitiveType());
            const char* primTypes[] = { "Cube", "Sphere", "Cylinder", "Plane" };
            if (ImGui::Combo("##PrimType", &currentType, primTypes, IM_ARRAYSIZE(primTypes))) {
                primNode->SetPrimitiveType(static_cast<MeshPrimitiveNode::PrimitiveType>(currentType));
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }


        ImGui::PopID();
    }

    // 5. Marquee (Box) Multi-Selection
    if (isWindowHovered && !anyCardHovered && !m_isDraggingLink && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_isBoxSelecting = true;
        m_boxSelectStart = mousePos;
        m_boxSelectEnd = mousePos;
        if (!ImGui::GetIO().KeyShift) {
            ClearSelection();
        }
    }

    if (m_isBoxSelecting) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_boxSelectEnd = mousePos;
            const ImVec2 minBox(std::min(m_boxSelectStart.x, m_boxSelectEnd.x), std::min(m_boxSelectStart.y, m_boxSelectEnd.y));
            const ImVec2 maxBox(std::max(m_boxSelectStart.x, m_boxSelectEnd.x), std::max(m_boxSelectStart.y, m_boxSelectEnd.y));

            drawList->AddRectFilled(minBox, maxBox, IM_COL32(0, 180, 255, 35));
            drawList->AddRect(minBox, maxBox, IM_COL32(0, 229, 255, 220), 0.0f, 0, 1.5f);

            // Compute overlapping nodes
            for (const auto& [nId, gNode] : m_graph->GetNodes()) {
                const ImVec2 nPos = CanvasToScreenSpace(GetNodePosition(nId), canvasOrigin);
                const ImVec2 nMin = nPos;
                const ImVec2 nMax(nPos.x + 240.0f * m_zoom, nPos.y + 120.0f * m_zoom);

                if (minBox.x <= nMax.x && maxBox.x >= nMin.x && minBox.y <= nMax.y && maxBox.y >= nMin.y) {
                    m_selectedNodeIds.insert(nId);
                    m_selectedNodeId = nId;
                }
            }
        } else {
            m_isBoxSelecting = false;
        }
    }

    // 6. Process Link Drag Drop Release / Auto-Wiring
    if (m_isDraggingLink && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (hoveredTargetPinId != 0 && hoveredTargetPinId != m_dragStartPinId) {
            const GraphPin* startPin  = m_graph->FindPin(m_dragStartPinId);
            const GraphPin* targetPin = m_graph->FindPin(hoveredTargetPinId);
            if (startPin && targetPin && startPin->direction != targetPin->direction) {
                const uint32_t outId = (startPin->direction == PinDirection::Output) ? startPin->id : targetPin->id;
                const uint32_t inId  = (startPin->direction == PinDirection::Input) ? startPin->id : targetPin->id;
                if (m_graph->Connect(outId, inId)) {
                    m_graph->Evaluate();
                    if (m_targetSceneNode) {
                        m_targetSceneNode->mesh = GetActiveOutputMesh();
                    }
                }
            }
        } else if (hoveredTargetPinId == 0) {
            // Dragged to empty space -> open pin drop creation context menu
            m_openPinDropPopup = true;
            m_droppedPinId = m_dragStartPinId;
            m_droppedCanvasPos = ScreenToCanvasSpace(mousePos, canvasOrigin);
        }
        m_isDraggingLink = false;
    }

    // Open Pin Drop Context Menu when wire released over empty space
    if (m_openPinDropPopup) {
        ImGui::OpenPopup("PinDropContextMenu");
        m_openPinDropPopup = false;
    }

    if (ImGui::BeginPopup("PinDropContextMenu")) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Add Node & Auto-Connect");
        ImGui::Separator();

        const graph::GraphPin* droppedPin = (m_droppedPinId != 0) ? m_graph->FindPin(m_droppedPinId) : nullptr;
        const graph::PinType  droppedType      = droppedPin ? droppedPin->type      : graph::PinType::GeometryBuffer;
        const graph::PinDirection droppedDir   = droppedPin ? droppedPin->direction : graph::PinDirection::Output;

        auto spawnAndConnect = [&](auto createFn) {
            auto node = createFn();
            m_nodePositions[node->GetId()] = m_droppedCanvasPos;
            SelectNode(node->GetId(), false);
            AutoConnectNodePin(m_droppedPinId, node->GetId());
        };

        bool anyShown = false;

        // GeometryBuffer OUTPUT -> spawn nodes that consume geometry
        if (droppedType == graph::PinType::GeometryBuffer && droppedDir == graph::PinDirection::Output) {
            if (ImGui::MenuItem("+ Subdivision Node")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<SubdivisionNode>(&m_context, 2); });
            }
            if (ImGui::MenuItem("+ Twist Deformer Node")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<TwistDeformerNode>(&m_context); });
            }

            anyShown = true;
        }

        // GeometryBuffer INPUT -> spawn nodes that produce geometry
        if (droppedType == graph::PinType::GeometryBuffer && droppedDir == graph::PinDirection::Input) {
            if (ImGui::MenuItem("+ Primitive (Cube)")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cube); });
            }
            if (ImGui::MenuItem("+ Primitive (Sphere)")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Sphere); });
            }
            if (ImGui::MenuItem("+ Primitive (Cylinder)")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cylinder); });
            }
            anyShown = true;
        }

        // Float INPUT -> add Float value node
        if (droppedType == graph::PinType::Float && droppedDir == graph::PinDirection::Input) {
            if (ImGui::MenuItem("+ Float Value Node")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<FloatNode>(1.0f); });
            }
            anyShown = true;
        }

        // Vector3 INPUT -> add Vector3 value node
        if (droppedType == graph::PinType::Vector3 && droppedDir == graph::PinDirection::Input) {
            if (ImGui::MenuItem("+ Vector3 Value Node")) {
                spawnAndConnect([&]{ return m_graph->CreateNode<Vector3Node>(glm::vec3(0.0f)); });
            }
            anyShown = true;
        }

        if (!anyShown) {
            ImGui::TextDisabled("No compatible nodes for this pin type.");
        }

        ImGui::EndPopup();
    }

    // 7. Canvas Right Click Menu
    if (isWindowHovered && !anyCardHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_isDraggingLink) {
        ImGui::OpenPopup("CanvasContextMenu");
    }

    if (ImGui::BeginPopup("CanvasContextMenu")) {
        const ImVec2 spawnCanvasPos = ScreenToCanvasSpace(mousePos, canvasOrigin);
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Add Graph Node");
        ImGui::Separator();

        if (ImGui::BeginMenu("Primitives")) {
            if (ImGui::MenuItem("+ Cube")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Cube);
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }
            if (ImGui::MenuItem("+ Sphere")) {
                auto node = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, MeshPrimitiveNode::PrimitiveType::Sphere);
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Deformers")) {
            if (ImGui::MenuItem("+ Subdivision Node")) {
                auto node = m_graph->CreateNode<SubdivisionNode>(&m_context, 2);
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }
            if (ImGui::MenuItem("+ Twist Deformer Node")) {
                auto node = m_graph->CreateNode<TwistDeformerNode>(&m_context);
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Value Nodes")) {
            if (ImGui::MenuItem("+ Float Value Node")) {
                auto node = m_graph->CreateNode<FloatNode>(1.0f);
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
            }
            if (ImGui::MenuItem("+ Vector3 Value Node")) {
                auto node = m_graph->CreateNode<Vector3Node>(glm::vec3(0.0f));
                m_nodePositions[node->GetId()] = spawnCanvasPos;
                SelectNode(node->GetId(), false);
                m_graph->Evaluate();
            }
            ImGui::EndMenu();
        }

        if (!m_selectedNodeIds.empty()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected Nodes")) {
                for (uint32_t delId : m_selectedNodeIds) {
                    m_graph->RemoveNode(delId);
                    m_nodePositions.erase(delId);
                }
                ClearSelection();
                m_graph->Evaluate();
                if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
            }
        }
        ImGui::EndPopup();
    }

    // 8. Delete Key Shortcut for Selected Nodes
    if (!m_selectedNodeIds.empty() && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        for (uint32_t delId : m_selectedNodeIds) {
            m_graph->RemoveNode(delId);
            m_nodePositions.erase(delId);
        }
        ClearSelection();
        m_graph->Evaluate();
        if (m_targetSceneNode) m_targetSceneNode->mesh = GetActiveOutputMesh();
    }

    drawList->PopClipRect();
}

} // namespace khepri


#include "NodeGraphEditorPanel.h"
#include "../graph/GeometryNodes.h"
#include "../core/Logger.h"

namespace khepri {

using namespace graph;

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

    m_graph->Evaluate();
    m_selectedNodeId = twistNode->GetId();
}

void NodeGraphEditorPanel::SetImportedMesh(std::shared_ptr<MeshComponent> importedMesh) {
    if (!importedMesh) return;

    std::shared_ptr<graph::ExternalMeshNode> extNode = nullptr;
    std::shared_ptr<graph::MeshPrimitiveNode> primNode = nullptr;

    for (const auto& [id, node] : m_graph->GetNodes()) {
        if (auto ext = std::dynamic_pointer_cast<graph::ExternalMeshNode>(node)) {
            extNode = ext;
        } else if (auto prim = std::dynamic_pointer_cast<graph::MeshPrimitiveNode>(node)) {
            primNode = prim;
        }
    }

    if (!extNode) {
        extNode = m_graph->CreateNode<graph::ExternalMeshNode>(importedMesh);
        if (primNode) {
            for (auto& [id, node] : m_graph->GetNodes()) {
                for (auto& input : node->GetInputs()) {
                    if (primNode->FindOutput("MeshBuffer") && input.connectedPinId == primNode->FindOutput("MeshBuffer")->id) {
                        m_graph->Disconnect(input.id);
                        (void)m_graph->Connect(extNode->FindOutput("MeshBuffer")->id, input.id);
                    }
                }
            }
            m_graph->RemoveNode(primNode->GetId());
        } else {
            for (const auto& [id, node] : m_graph->GetNodes()) {
                if (auto subNode = std::dynamic_pointer_cast<graph::SubdivisionNode>(node)) {
                    (void)m_graph->Connect(extNode->FindOutput("MeshBuffer")->id, subNode->FindInput("MeshBuffer")->id);
                    break;
                } else if (auto twistNode = std::dynamic_pointer_cast<graph::TwistDeformerNode>(node)) {
                    (void)m_graph->Connect(extNode->FindOutput("MeshBuffer")->id, twistNode->FindInput("MeshBuffer")->id);
                    break;
                }
            }
        }
    } else {
        extNode->SetExternalMesh(importedMesh);
    }

    m_selectedNodeId = extNode->GetId();
    m_graph->Evaluate();
}

void NodeGraphEditorPanel::RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh) {
    ImGui::Begin("Procedural Node Graph Editor");

    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Khepri Procedural Graph Engine (Layer 2 Core)");
    ImGui::Separator();

    RenderToolbar();
    ImGui::Separator();

    ImGui::Columns(2, "NodeEditorColumns", true);

    RenderNodeCanvas();
    ImGui::NextColumn();

    RenderNodeProperties();
    ImGui::Columns(1);

    std::shared_ptr<MeshComponent> selectedOutputMesh = nullptr;
    std::shared_ptr<MeshComponent> lastOutputMesh = nullptr;

    for (const auto& [id, node] : m_graph->GetNodes()) {
        std::shared_ptr<MeshComponent> nodeMesh = nullptr;
        if (auto primNode = std::dynamic_pointer_cast<MeshPrimitiveNode>(node)) nodeMesh = primNode->GetOutputMesh();
        else if (auto extNode = std::dynamic_pointer_cast<graph::ExternalMeshNode>(node)) nodeMesh = extNode->GetOutputMesh();
        else if (auto subNode = std::dynamic_pointer_cast<SubdivisionNode>(node)) nodeMesh = subNode->GetOutputMesh();
        else if (auto twistNode = std::dynamic_pointer_cast<TwistDeformerNode>(node)) nodeMesh = twistNode->GetOutputMesh();
        else if (auto csgNode = std::dynamic_pointer_cast<CSGBooleanNode>(node)) nodeMesh = csgNode->GetOutputMesh();

        if (nodeMesh) {
            lastOutputMesh = nodeMesh;
            if (m_selectedNodeId == id) {
                selectedOutputMesh = nodeMesh;
            }
        }
    }

    if (selectedOutputMesh) {
        activeDisplayMesh = selectedOutputMesh;
    } else if (lastOutputMesh) {
        activeDisplayMesh = lastOutputMesh;
    }

    ImGui::End();
}

void NodeGraphEditorPanel::RenderToolbar() {
    if (ImGui::Button("+ Primitive Node")) {
        const auto primType = (m_primitiveTypeCombo == 0) ? MeshPrimitiveNode::PrimitiveType::Cube :
                              (m_primitiveTypeCombo == 1) ? MeshPrimitiveNode::PrimitiveType::Sphere :
                              (m_primitiveTypeCombo == 2) ? MeshPrimitiveNode::PrimitiveType::Cylinder :
                                                            MeshPrimitiveNode::PrimitiveType::Plane;
        auto newNode = m_graph->CreateNode<MeshPrimitiveNode>(&m_context, primType);
        m_selectedNodeId = newNode->GetId();
        m_graph->Evaluate();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##PrimitiveType", &m_primitiveTypeCombo, "Cube\0Sphere\0Cylinder\0Plane\0");

    ImGui::SameLine();
    if (ImGui::Button("+ Imported Mesh")) {
        auto newNode = m_graph->CreateNode<graph::ExternalMeshNode>(nullptr);
        m_selectedNodeId = newNode->GetId();
        m_graph->Evaluate();
    }

    ImGui::SameLine();
    if (ImGui::Button("+ Subdivide Node")) {
        auto newNode = m_graph->CreateNode<SubdivisionNode>(&m_context, 2);
        m_selectedNodeId = newNode->GetId();
        m_graph->Evaluate();
    }

    ImGui::SameLine();
    if (ImGui::Button("+ Twist Node")) {
        auto newNode = m_graph->CreateNode<TwistDeformerNode>(&m_context);
        m_selectedNodeId = newNode->GetId();
        m_graph->Evaluate();
    }

    ImGui::SameLine();
    if (ImGui::Button("Evaluate Graph")) {
        m_graph->Evaluate();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Verbose Logs", &graph::GraphNode::s_enableGraphLogging);
}

void NodeGraphEditorPanel::RenderNodeCanvas() {
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Graph Nodes & Pin Connections:");
    ImGui::Separator();

    // Collect all available output pins
    struct AvailableOutPin {
        uint32_t pinId;
        std::string label;
    };
    std::vector<AvailableOutPin> availableOutputs;
    for (const auto& [id, node] : m_graph->GetNodes()) {
        for (const auto& outPin : node->GetOutputs()) {
            availableOutputs.push_back({ outPin.id, "[" + std::to_string(node->GetId()) + " " + node->GetName() + "] " + outPin.name });
        }
    }

    for (const auto& [id, node] : m_graph->GetNodes()) {
        ImGui::PushID(static_cast<int>(id));
        const std::string label = "[" + std::to_string(id) + "] " + node->GetName();
        const bool isSelected = (m_selectedNodeId == id);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            m_selectedNodeId = id;
        }

        for (auto& pin : node->GetInputs()) {
            ImGui::BulletText(" In: %s", pin.name.c_str());
            ImGui::SameLine();
            if (pin.connectedPinId != 0) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Connected)");
                ImGui::SameLine();
                if (ImGui::SmallButton("Disconnect")) {
                    m_graph->Disconnect(pin.id);
                    m_graph->Evaluate();
                }
            } else {
                if (ImGui::BeginCombo("##conn", "Connect to...")) {
                    for (const auto& outPin : availableOutputs) {
                        if (outPin.pinId != pin.id) { // Cannot connect to self
                            if (ImGui::Selectable(outPin.label.c_str())) {
                                (void)m_graph->Connect(outPin.pinId, pin.id);
                                m_graph->Evaluate();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        for (const auto& pin : node->GetOutputs()) {
            ImGui::BulletText(" Out: %s", pin.name.c_str());
        }
        ImGui::PopID();
        ImGui::Separator();
    }
}

void NodeGraphEditorPanel::RenderNodeProperties() {
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Node Properties:");
    ImGui::Separator();

    if (m_selectedNodeId == 0) {
        ImGui::TextDisabled("Select a node from the canvas to edit parameters.");
        return;
    }

    auto node = m_graph->GetNode(m_selectedNodeId);
    if (!node) return;

    ImGui::Text("Node Name: %s", node->GetName().c_str());
    ImGui::Text("Node ID: %u", node->GetId());
    ImGui::Separator();

    if (auto primNode = std::dynamic_pointer_cast<MeshPrimitiveNode>(node)) {
        int currentType = static_cast<int>(primNode->GetPrimitiveType());
        const char* primTypes[] = { "Cube", "Sphere", "Cylinder", "Plane" };
        if (ImGui::Combo("Primitive Type", &currentType, primTypes, IM_ARRAYSIZE(primTypes))) {
            primNode->SetPrimitiveType(static_cast<MeshPrimitiveNode::PrimitiveType>(currentType));
            m_graph->Evaluate();
        }

        if (primNode->GetPrimitiveType() == MeshPrimitiveNode::PrimitiveType::Cube) {
            int segs = static_cast<int>(primNode->GetSegmentsY());
            if (ImGui::DragInt("Segments Y (Height Resolution)", &segs, 1, 1, 50)) {
                primNode->SetSegmentsY(static_cast<uint32_t>(segs));
                m_graph->Evaluate();
            }
        }
    } else if (auto subdivNode = std::dynamic_pointer_cast<SubdivisionNode>(node)) {
        int level = static_cast<int>(subdivNode->GetSubdivisionLevel());
        if (ImGui::DragInt("Subdivision Level", &level, 1, 0, 4)) {
            subdivNode->SetSubdivisionLevel(static_cast<uint32_t>(level));
            m_graph->Evaluate();
        }
    } else if (auto twistNode = std::dynamic_pointer_cast<TwistDeformerNode>(node)) {
        float angle = twistNode->GetAngle();
        if (ImGui::DragFloat("Twist Angle (deg)", &angle, 1.0f, -360.0f, 360.0f)) {
            twistNode->SetAngle(angle);
            m_graph->Evaluate();
        }
    }
}

} // namespace khepri

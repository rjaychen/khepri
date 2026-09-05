#include "SceneNode.h"
#include <algorithm>

uint32_t SceneNode::s_nextId = 1;

SceneNode::SceneNode(const std::string& nodeName)
    : name(nodeName), id(s_nextId++) {
    
    // Set up default inspectable properties for Editor Property Inspector
    Property propPos;
    propPos.name = "Position";
    propPos.type = PropertyType::Vec3;
    propPos.value = position;
    propPos.minVal = -100.0f; propPos.maxVal = 100.0f;
    propPos.onChangeCallback = [this]() { SyncTransformToProperties(); };
    m_properties.push_back(propPos);

    Property propRot;
    propRot.name = "Rotation";
    propRot.type = PropertyType::Vec3;
    propRot.value = rotationDegrees;
    propRot.minVal = -360.0f; propRot.maxVal = 360.0f;
    propRot.onChangeCallback = [this]() { SyncTransformToProperties(); };
    m_properties.push_back(propRot);

    Property propScale;
    propScale.name = "Scale";
    propScale.type = PropertyType::Vec3;
    propScale.value = scale;
    propScale.minVal = 0.001f; propScale.maxVal = 100.0f;
    propScale.onChangeCallback = [this]() { SyncTransformToProperties(); };
    m_properties.push_back(propScale);
}

glm::mat4 SceneNode::GetLocalTransform() const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 r = glm::toMat4(glm::quat(glm::radians(rotationDegrees)));
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

glm::mat4 SceneNode::GetWorldTransform() const {
    if (m_parent) {
        return m_parent->GetWorldTransform() * GetLocalTransform();
    }
    return GetLocalTransform();
}

SceneNode* SceneNode::AddChild(std::unique_ptr<SceneNode> child) {
    child->m_parent = this;
    SceneNode* rawPtr = child.get();
    m_children.push_back(std::move(child));
    return rawPtr;
}

void SceneNode::RemoveChild(SceneNode* child) {
    auto it = std::remove_if(m_children.begin(), m_children.end(),
        [child](const std::unique_ptr<SceneNode>& ptr) { return ptr.get() == child; });
    m_children.erase(it, m_children.end());
}

std::unique_ptr<SceneNode> SceneNode::DetachChild(SceneNode* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const std::unique_ptr<SceneNode>& ptr) { return ptr.get() == child; });
    if (it != m_children.end()) {
        std::unique_ptr<SceneNode> detached = std::move(*it);
        m_children.erase(it);
        detached->m_parent = nullptr;
        return detached;
    }
    return nullptr;
}

bool SceneNode::IsDescendantOf(const SceneNode* possibleAncestor) const {
    if (!possibleAncestor) return false;
    const SceneNode* curr = m_parent;
    while (curr) {
        if (curr == possibleAncestor) return true;
        curr = curr->m_parent;
    }
    return false;
}

bool SceneNode::Contains(const SceneNode* target) const noexcept {
    if (!target) return false;
    if (target == this) return true;
    for (const auto& child : m_children) {
        if (child && child->Contains(target)) return true;
    }
    return false;
}

void SceneNode::SyncPropertiesToTransform() {
    if (m_properties.size() >= 3) {
        m_properties[0].value = position;
        m_properties[1].value = rotationDegrees;
        m_properties[2].value = scale;
    }
}

void SceneNode::SyncTransformToProperties() {
    if (m_properties.size() >= 3) {
        position = std::get<glm::vec3>(m_properties[0].value);
        rotationDegrees = std::get<glm::vec3>(m_properties[1].value);
        scale = std::get<glm::vec3>(m_properties[2].value);
    }
}

#include "../graph/NodeGraph.h"
#include "../graph/GeometryNodes.h"

std::shared_ptr<khepri::graph::NodeGraph> SceneNode::GetOrCreateNodeGraph(VulkanContext* context) {
    if (!nodeGraph) {
        nodeGraph = std::make_shared<khepri::graph::NodeGraph>();
        if (mesh) {
            // Seed graph with an ExternalMeshNode referencing the initial mesh
            auto extNode = nodeGraph->CreateNode<khepri::graph::ExternalMeshNode>(mesh);
            (void)extNode;
        } else if (context && !IsLightNode() && !lightComponent) {
            auto primNode = nodeGraph->CreateNode<khepri::graph::MeshPrimitiveNode>(context, khepri::graph::MeshPrimitiveNode::PrimitiveType::Cube);
            (void)primNode;
        }
        EvaluateNodeGraph(false);
    }
    return nodeGraph;
}

void SceneNode::EvaluateNodeGraph(bool propagateToChildren) {
    if (nodeGraph && !IsLightNode() && !lightComponent) {
        nodeGraph->Evaluate();

        // Find terminal geometry output or last produced mesh
        std::shared_ptr<MeshComponent> outMesh = nullptr;
        for (const auto& [id, gNode] : nodeGraph->GetNodes()) {
            if (auto m = gNode->GetOutputMesh()) {
                outMesh = m;
            }
        }
        if (outMesh) {
            mesh = outMesh;
        }
    }

    if (propagateToChildren) {
        for (auto& child : m_children) {
            if (child) {
                child->EvaluateNodeGraph(true);
            }
        }
    }
}

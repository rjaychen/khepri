#include "GeometryNodes.h"
#include "../core/Logger.h"

namespace khepri::graph {

MeshPrimitiveNode::MeshPrimitiveNode(uint32_t id, VulkanContext* context, PrimitiveType type) noexcept
    : GraphNode(id, "Mesh Primitive", NodeDomain::Geometry), m_context(context), m_primitiveType(type) {
    AddOutput("MeshBuffer", PinType::GeometryBuffer);
}

void MeshPrimitiveNode::Evaluate() {
    switch (m_primitiveType) {
    case PrimitiveType::Cube:
        m_outputMesh = MeshComponent::CreateCube(m_context, 2.0f, m_segmentsX, m_segmentsY, m_segmentsZ);
        break;
    case PrimitiveType::Sphere:
        m_outputMesh = MeshComponent::CreateSphere(*m_context, 1.2f, 32, 16);
        break;
   // Generates primitive mesh buffers (Cube, Sphere, Cylinder, Plane) - Procedural Geometry Nodes:
    case PrimitiveType::Cylinder:
        m_outputMesh = MeshComponent::CreateCylinder(*m_context, 0.8f, 2.0f, 32);
        break;
    case PrimitiveType::Plane:
        m_outputMesh = MeshComponent::CreatePlane(*m_context, 4.0f, 8);
        break;
    }
    auto* outPin = FindOutput("MeshBuffer");
    if (outPin) outPin->value = m_outputMesh;
    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated MeshPrimitiveNode " + std::to_string(m_id));
    }
}

ExternalMeshNode::ExternalMeshNode(uint32_t id, std::shared_ptr<MeshComponent> externalMesh) noexcept
    : GraphNode(id, "Imported Mesh", NodeDomain::Geometry), m_outputMesh(externalMesh) {
    AddOutput("MeshBuffer", PinType::GeometryBuffer);
}

void ExternalMeshNode::Evaluate() {
    auto* outPin = FindOutput("MeshBuffer");
    if (outPin) outPin->value = m_outputMesh;
    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated ExternalMeshNode " + std::to_string(m_id));
    }
}

SubdivisionNode::SubdivisionNode(uint32_t id, VulkanContext* context, uint32_t levels) noexcept
    : GraphNode(id, "Subdivision", NodeDomain::Geometry), m_context(context), m_levels(levels) {
    AddInput("MeshBuffer", PinType::GeometryBuffer);
    AddInput("Level", PinType::Float, static_cast<float>(levels));
    AddOutput("SubdividedMeshBuffer", PinType::GeometryBuffer);
}

void SubdivisionNode::Evaluate() {
    const auto* lvlPin = FindInput("Level");
    if (lvlPin && std::holds_alternative<float>(lvlPin->value)) {
        m_levels = static_cast<uint32_t>(std::max(0.0f, std::get<float>(lvlPin->value)));
    }

    std::shared_ptr<MeshComponent> inputMesh = nullptr;
    const auto* inMeshPin = FindInput("MeshBuffer");
    if (inMeshPin && std::holds_alternative<std::shared_ptr<MeshComponent>>(inMeshPin->value)) {
        inputMesh = std::get<std::shared_ptr<MeshComponent>>(inMeshPin->value);
    }

    if (!inputMesh) {
        inputMesh = MeshComponent::CreateCube(m_context, 2.0f, 1, 1, 1);
    }

    m_outputMesh = MeshComponent::SubdivideMesh(m_context, *inputMesh, m_levels);

    auto* outPin = FindOutput("SubdividedMeshBuffer");
    if (outPin) outPin->value = m_outputMesh;

    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated SubdivisionNode " + std::to_string(m_id) + " with level " + std::to_string(m_levels));
    }
}

TwistDeformerNode::TwistDeformerNode(uint32_t id, VulkanContext* context) noexcept
    : GraphNode(id, "Twist Deformer", NodeDomain::Geometry), m_context(context) {
    AddInput("MeshBuffer", PinType::GeometryBuffer);
    AddInput("Angle", PinType::Float, 45.0f);
    AddOutput("DeformedMeshBuffer", PinType::GeometryBuffer);
}

void TwistDeformerNode::Evaluate() {
    const auto* anglePin = FindInput("Angle");
    if (anglePin && std::holds_alternative<float>(anglePin->value)) {
        m_angle = std::get<float>(anglePin->value);
    }

    std::shared_ptr<MeshComponent> inputMesh = nullptr;
    const auto* inMeshPin = FindInput("MeshBuffer");
    if (inMeshPin && std::holds_alternative<std::shared_ptr<MeshComponent>>(inMeshPin->value)) {
        inputMesh = std::get<std::shared_ptr<MeshComponent>>(inMeshPin->value);
    }

    if (!inputMesh) {
        inputMesh = MeshComponent::CreateCube(m_context, 2.0f, 10, 10, 10);
    }

    std::vector<Vertex> verts = inputMesh->GetVertices();
    const std::vector<uint32_t>& indices = inputMesh->GetIndices();

    if (!verts.empty()) {
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        for (const auto& v : verts) {
            minY = std::min(minY, v.position.y);
            maxY = std::max(maxY, v.position.y);
        }

        float height = maxY - minY;
        if (height < 1e-5f) height = 1.0f;

        for (auto& v : verts) {
            float factor = (v.position.y - minY) / height;
            float angleRad = glm::radians(m_angle) * factor;
            float cosA = std::cos(angleRad);
            float sinA = std::sin(angleRad);

            float x = v.position.x;
            float z = v.position.z;
            v.position.x = x * cosA - z * sinA;
            v.position.z = x * sinA + z * cosA;

            float nx = v.normal.x;
            float nz = v.normal.z;
            v.normal.x = nx * cosA - nz * sinA;
            v.normal.z = nx * sinA + nz * cosA;
            if (glm::length(v.normal) > 1e-5f) {
                v.normal = glm::normalize(v.normal);
            }
        }
    }

    m_outputMesh = std::make_shared<MeshComponent>(m_context, verts, indices);
    m_outputMesh->SetTexture(inputMesh->GetTexture());
    m_outputMesh->SetBaseColorFactor(inputMesh->GetBaseColorFactor());

    auto* outPin = FindOutput("DeformedMeshBuffer");
    if (outPin) outPin->value = m_outputMesh;

    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated TwistDeformerNode " + std::to_string(m_id) + " with angle " + std::to_string(m_angle));
    }
}

FloatNode::FloatNode(uint32_t id, float initialValue) noexcept
    : GraphNode(id, "Float Value", NodeDomain::Geometry), m_value(initialValue) {
    AddOutput("Value", PinType::Float);
}

void FloatNode::Evaluate() {
    auto* outPin = FindOutput("Value");
    if (outPin) outPin->value = m_value;
    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated FloatNode " + std::to_string(m_id) + " value=" + std::to_string(m_value));
    }
}

Vector3Node::Vector3Node(uint32_t id, const glm::vec3& initialValue) noexcept
    : GraphNode(id, "Vector3 Value", NodeDomain::Geometry), m_value(initialValue) {
    AddOutput("Value", PinType::Vector3);
}

void Vector3Node::Evaluate() {
    auto* outPin = FindOutput("Value");
    if (outPin) outPin->value = m_value;
    if (s_enableGraphLogging) {
        LOG_INFO("Evaluated Vector3Node " + std::to_string(m_id));
    }
}

} // namespace khepri::graph

#pragma once

#include "NodeGraph.h"
#include "../scene/MeshComponent.h"
#include <memory>
#include <algorithm>

namespace khepri::graph {

// Generates primitive mesh buffers (Cube, Sphere, Cylinder, Plane)
class MeshPrimitiveNode : public GraphNode {
public:
    enum class PrimitiveType { Cube, Sphere, Cylinder, Plane };

    MeshPrimitiveNode(uint32_t id, VulkanContext* context = nullptr, PrimitiveType type = PrimitiveType::Cube) noexcept;
    
    void Evaluate() override;

    [[nodiscard]] PrimitiveType GetPrimitiveType() const noexcept { return m_primitiveType; }
    void SetPrimitiveType(PrimitiveType type) noexcept { m_primitiveType = type; MarkDirty(); }

    [[nodiscard]] uint32_t GetSegmentsX() const noexcept { return m_segmentsX; }
    [[nodiscard]] uint32_t GetSegmentsY() const noexcept { return m_segmentsY; }
    [[nodiscard]] uint32_t GetSegmentsZ() const noexcept { return m_segmentsZ; }
    void SetSegmentsX(uint32_t segs) noexcept { m_segmentsX = std::max(1u, segs); MarkDirty(); }
    void SetSegmentsY(uint32_t segs) noexcept { m_segmentsY = std::max(1u, segs); MarkDirty(); }
    void SetSegmentsZ(uint32_t segs) noexcept { m_segmentsZ = std::max(1u, segs); MarkDirty(); }
    void SetSegments(uint32_t x, uint32_t y, uint32_t z) noexcept {
        m_segmentsX = std::max(1u, x);
        m_segmentsY = std::max(1u, y);
        m_segmentsZ = std::max(1u, z);
        MarkDirty();
    }

    [[nodiscard]] std::shared_ptr<MeshComponent> GetOutputMesh() const noexcept { return m_outputMesh; }

private:
    VulkanContext* m_context{nullptr};
    PrimitiveType m_primitiveType;
    uint32_t m_segmentsX{10};
    uint32_t m_segmentsY{10};
    uint32_t m_segmentsZ{10};
    std::shared_ptr<MeshComponent> m_outputMesh{nullptr};
};

// Feeds an externally imported model/mesh (glTF, OBJ, STL) into procedural graph dataflow
class ExternalMeshNode : public GraphNode {
public:
    ExternalMeshNode(uint32_t id, std::shared_ptr<MeshComponent> externalMesh = nullptr) noexcept;

    void Evaluate() override;

    void SetExternalMesh(std::shared_ptr<MeshComponent> mesh) noexcept {
        m_outputMesh = mesh;
        MarkDirty();
    }

    [[nodiscard]] std::shared_ptr<MeshComponent> GetOutputMesh() const noexcept { return m_outputMesh; }

private:
    std::shared_ptr<MeshComponent> m_outputMesh{nullptr};
};

// Procedural Subdivision Node (1-to-4 midpoint triangle subdivision)
class SubdivisionNode : public GraphNode {
public:
    SubdivisionNode(uint32_t id, VulkanContext* context = nullptr, uint32_t levels = 2) noexcept;

    void Evaluate() override;

    [[nodiscard]] uint32_t GetSubdivisionLevel() const noexcept { return m_levels; }
    void SetSubdivisionLevel(uint32_t levels) noexcept {
        m_levels = std::clamp(levels, 0u, 5u);
        auto* lvlPin = FindInput("Level");
        if (lvlPin) {
            lvlPin->value = static_cast<float>(m_levels);
        }
        MarkDirty();
    }

    [[nodiscard]] std::shared_ptr<MeshComponent> GetOutputMesh() const noexcept { return m_outputMesh; }

private:
    VulkanContext* m_context{nullptr};
    uint32_t m_levels{2};
    std::shared_ptr<MeshComponent> m_outputMesh{nullptr};
};

// Deforms input mesh vertices and normals along Y height axis
class TwistDeformerNode : public GraphNode {
public:
    TwistDeformerNode(uint32_t id, VulkanContext* context = nullptr) noexcept;
    
    void Evaluate() override;

    [[nodiscard]] float GetAngle() const noexcept { return m_angle; }
    void SetAngle(float angle) noexcept {
        m_angle = angle;
        auto* anglePin = FindInput("Angle");
        if (anglePin) {
            anglePin->value = angle;
        }
        MarkDirty();
    }

    [[nodiscard]] std::shared_ptr<MeshComponent> GetOutputMesh() const noexcept { return m_outputMesh; }

private:
    VulkanContext* m_context{nullptr};
    float m_angle{45.0f}; // degrees
    std::shared_ptr<MeshComponent> m_outputMesh{nullptr};
};

// Subsumes CSG Boolean operations into a procedural node
class CSGBooleanNode : public GraphNode {
public:
    enum class OpType { Union, Intersection, Difference };

    CSGBooleanNode(uint32_t id, VulkanContext* context = nullptr, OpType op = OpType::Union) noexcept;

    void Evaluate() override;

    [[nodiscard]] std::shared_ptr<MeshComponent> GetOutputMesh() const noexcept { return m_outputMesh; }

private:
    VulkanContext* m_context{nullptr};
    OpType m_opType;
    std::shared_ptr<MeshComponent> m_outputMesh{nullptr};
};

} // namespace khepri::graph

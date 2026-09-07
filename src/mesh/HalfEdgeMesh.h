#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <limits>
#include <span>
#include "../core/StrongId.h"
#include "../scene/MeshComponent.h"

namespace khepri {

struct VertexTag {};
struct HalfEdgeTag {};
struct FaceTag {};

using VertexId   = StrongId<VertexTag, uint32_t>;
using HalfEdgeId = StrongId<HalfEdgeTag, uint32_t>;
using FaceId     = StrongId<FaceTag, uint32_t>;

inline constexpr VertexId   InvalidVertexId   = VertexId::Invalid();
inline constexpr HalfEdgeId InvalidHalfEdgeId = HalfEdgeId::Invalid();
inline constexpr FaceId     InvalidFaceId     = FaceId::Invalid();

} // namespace khepri

constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

struct HE_Vertex {
    khepri::VertexId id = khepri::InvalidVertexId;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f, 0.0f};
    khepri::HalfEdgeId halfEdge = khepri::InvalidHalfEdgeId; // Outgoing half-edge
};

struct HE_HalfEdge {
    khepri::HalfEdgeId id = khepri::InvalidHalfEdgeId;
    khepri::VertexId origin = khepri::InvalidVertexId;      // Origin vertex
    khepri::HalfEdgeId twin = khepri::InvalidHalfEdgeId;    // Twin half-edge
    khepri::HalfEdgeId next = khepri::InvalidHalfEdgeId;    // Next half-edge
    khepri::FaceId face = khepri::InvalidFaceId;            // Face
};

struct HE_Face {
    khepri::FaceId id = khepri::InvalidFaceId;
    khepri::HalfEdgeId halfEdge = khepri::InvalidHalfEdgeId; // Boundary half-edge
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

class HalfEdgeMesh {
public:
    HalfEdgeMesh() = default;
    ~HalfEdgeMesh() = default;

    void Clear();
    void BuildFromIndexedMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    
    // Flatten index-based authoring mesh into render mesh buffers (Mesh Baking Routine)
    void BakeToRenderMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const;
    void ToIndexedMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const {
        BakeToRenderMesh(outVertices, outIndices);
    }

    khepri::VertexId AddVertex(const glm::vec3& pos, const glm::vec3& norm = glm::vec3(0, 1, 0), const glm::vec2& uv = glm::vec2(0));
    khepri::FaceId AddTriangle(khepri::VertexId v0, khepri::VertexId v1, khepri::VertexId v2);
    khepri::FaceId AddTriangle(uint32_t v0Idx, uint32_t v1Idx, uint32_t v2Idx) {
        return AddTriangle(khepri::VertexId(v0Idx), khepri::VertexId(v1Idx), khepri::VertexId(v2Idx));
    }

    // Topological Mesh Operators
    // NOTE [SplitEdge / Reference Invalidation Fragility]:
    // Mutating operations (AddVertex, SplitEdge, FlipEdge) push to m_vertices, m_halfEdges,
    // and m_faces vectors. This reallocates vector buffers and invalidates any raw C++ references
    // or pointers held by callers. All indexing and state tracking MUST use strongly-typed IDs
    // (VertexId, HalfEdgeId, FaceId) rather than raw pointers across mutating calls.
    bool FlipEdge(khepri::HalfEdgeId edgeId);
    bool FlipEdge(uint32_t edgeIdx) { return FlipEdge(khepri::HalfEdgeId(edgeIdx)); }

    khepri::VertexId SplitEdge(khepri::HalfEdgeId edgeId, const glm::vec3& newPos);
    uint32_t SplitEdge(uint32_t edgeIdx, const glm::vec3& newPos) {
        return SplitEdge(khepri::HalfEdgeId(edgeIdx), newPos).Get();
    }

    [[nodiscard]] const std::vector<HE_Vertex>& GetVertices() const noexcept { return m_vertices; }
    [[nodiscard]] const std::vector<HE_HalfEdge>& GetHalfEdges() const noexcept { return m_halfEdges; }
    [[nodiscard]] const std::vector<HE_Face>& GetFaces() const noexcept { return m_faces; }

    [[nodiscard]] size_t GetVertexCount() const noexcept { return m_vertices.size(); }
    [[nodiscard]] size_t GetHalfEdgeCount() const noexcept { return m_halfEdges.size(); }
    [[nodiscard]] size_t GetFaceCount() const noexcept { return m_faces.size(); }
    [[nodiscard]] const glm::vec3& GetVertexPosition(khepri::VertexId id) const { return m_vertices[id.Get()].position; }

    [[nodiscard]] uint32_t GetEulerCharacteristic() const;

    [[nodiscard]] uint32_t GetFaceAcross(uint32_t edgeIdx) const;
    [[nodiscard]] khepri::FaceId GetFaceAcross(khepri::HalfEdgeId edgeId) const;

    // Fast O(deg(v)) 1-ring vertex neighbor traversal using half-edge twin/next cycles.
    [[nodiscard]] std::vector<khepri::VertexId> GetVertexNeighborIds(khepri::VertexId vertexId) const;
    [[nodiscard]] std::vector<uint32_t> GetVertexNeighbors(uint32_t vertexIdx) const;

private:
    std::vector<HE_Vertex> m_vertices;
    std::vector<HE_HalfEdge> m_halfEdges;
    std::vector<HE_Face> m_faces;

    struct PairHash {
        std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const noexcept {
            return std::hash<uint32_t>()(p.first) ^ (std::hash<uint32_t>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, PairHash> m_edgeMap;
};


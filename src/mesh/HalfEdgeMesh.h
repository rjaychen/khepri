#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <limits>
#include "../scene/MeshComponent.h"

constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

struct HE_Vertex {
    uint32_t id = INVALID_INDEX;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f, 0.0f};
    uint32_t halfEdge = INVALID_INDEX; // Outgoing half-edge index
};

struct HE_HalfEdge {
    uint32_t id = INVALID_INDEX;
    uint32_t origin = INVALID_INDEX; // Origin vertex index
    uint32_t twin = INVALID_INDEX;   // Twin half-edge index
    uint32_t next = INVALID_INDEX;   // Next half-edge index
    uint32_t face = INVALID_INDEX;   // Face index
};

struct HE_Face {
    uint32_t id = INVALID_INDEX;
    uint32_t halfEdge = INVALID_INDEX; // Boundary half-edge index
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

    uint32_t AddVertex(const glm::vec3& pos, const glm::vec3& norm = glm::vec3(0, 1, 0), const glm::vec2& uv = glm::vec2(0));
    uint32_t AddTriangle(uint32_t v0Idx, uint32_t v1Idx, uint32_t v2Idx);

    // Topological Mesh Operators
    bool FlipEdge(uint32_t edgeIdx);
    uint32_t SplitEdge(uint32_t edgeIdx, const glm::vec3& newPos);

    const std::vector<HE_Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<HE_HalfEdge>& GetHalfEdges() const { return m_halfEdges; }
    const std::vector<HE_Face>& GetFaces() const { return m_faces; }

    uint32_t GetEulerCharacteristic() const;

    uint32_t GetFaceAcross(uint32_t edgeIdx);
    std::vector<uint32_t> GetVertexNeighbors(uint32_t vertexIdx);

private:
    std::vector<HE_Vertex> m_vertices;
    std::vector<HE_HalfEdge> m_halfEdges;
    std::vector<HE_Face> m_faces;

    struct PairHash {
        std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
            return std::hash<uint32_t>()(p.first) ^ (std::hash<uint32_t>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, PairHash> m_edgeMap;
};

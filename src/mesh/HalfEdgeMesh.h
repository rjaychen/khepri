#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../scene/MeshComponent.h"

struct HE_Vertex;
struct HE_HalfEdge;
struct HE_Face;

struct HE_Vertex {
    uint32_t id;
    glm::vec3 position;
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f, 0.0f};
    HE_HalfEdge* halfEdge = nullptr; // Outgoing half-edge
};

struct HE_HalfEdge {
    uint32_t id;
    HE_Vertex* origin = nullptr;
    HE_HalfEdge* twin = nullptr;
    HE_HalfEdge* next = nullptr;
    HE_Face* face = nullptr;
};

struct HE_Face {
    uint32_t id;
    HE_HalfEdge* halfEdge = nullptr; // One of the half-edges on boundary loop
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

class HalfEdgeMesh {
public:
    HalfEdgeMesh() = default;
    ~HalfEdgeMesh();

    HalfEdgeMesh(const HalfEdgeMesh&) = delete;
    HalfEdgeMesh& operator=(const HalfEdgeMesh&) = delete;

    void Clear();
    void BuildFromIndexedMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void ToIndexedMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const;

    HE_Vertex* AddVertex(const glm::vec3& pos, const glm::vec3& norm = glm::vec3(0, 1, 0), const glm::vec2& uv = glm::vec2(0));
    HE_Face* AddTriangle(HE_Vertex* v0, HE_Vertex* v1, HE_Vertex* v2);

    // Topological Mesh Operators
    bool FlipEdge(HE_HalfEdge* edge);
    HE_Vertex* SplitEdge(HE_HalfEdge* edge, const glm::vec3& newPos);

    const std::vector<std::unique_ptr<HE_Vertex>>& GetVertices() const { return m_vertices; }
    const std::vector<std::unique_ptr<HE_HalfEdge>>& GetHalfEdges() const { return m_halfEdges; }
    const std::vector<std::unique_ptr<HE_Face>>& GetFaces() const { return m_faces; }

    uint32_t GetEulerCharacteristic() const;

private:
    std::vector<std::unique_ptr<HE_Vertex>> m_vertices;
    std::vector<std::unique_ptr<HE_HalfEdge>> m_halfEdges;
    std::vector<std::unique_ptr<HE_Face>> m_faces;

    struct PairHash {
        std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
            return std::hash<uint32_t>()(p.first) ^ (std::hash<uint32_t>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<uint32_t, uint32_t>, HE_HalfEdge*, PairHash> m_edgeMap;
};

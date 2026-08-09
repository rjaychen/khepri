#include "HalfEdgeMesh.h"
#include "../core/Logger.h"
#include <algorithm>

void HalfEdgeMesh::Clear() {
    m_vertices.clear();
    m_halfEdges.clear();
    m_faces.clear();
    m_edgeMap.clear();
}

uint32_t HalfEdgeMesh::AddVertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv) {
    HE_Vertex v;
    v.id = static_cast<uint32_t>(m_vertices.size());
    v.position = pos;
    v.normal = norm;
    v.uv = uv;
    v.halfEdge = INVALID_INDEX;
    m_vertices.push_back(v);
    return v.id;
}

uint32_t HalfEdgeMesh::AddTriangle(uint32_t v0Idx, uint32_t v1Idx, uint32_t v2Idx) {
    if (v0Idx >= m_vertices.size() || v1Idx >= m_vertices.size() || v2Idx >= m_vertices.size()) {
        return INVALID_INDEX;
    }

    HE_Face face;
    face.id = static_cast<uint32_t>(m_faces.size());
    
    glm::vec3 n = glm::cross(m_vertices[v1Idx].position - m_vertices[v0Idx].position,
                            m_vertices[v2Idx].position - m_vertices[v0Idx].position);
    face.normal = (glm::length(n) > 1e-7f) ? glm::normalize(n) : glm::vec3(0, 1, 0);

    uint32_t vIndices[3] = { v0Idx, v1Idx, v2Idx };
    uint32_t edgeIndices[3];

    for (int i = 0; i < 3; ++i) {
        HE_HalfEdge e;
        e.id = static_cast<uint32_t>(m_halfEdges.size());
        e.origin = vIndices[i];
        e.face = face.id;
        edgeIndices[i] = e.id;
        
        if (m_vertices[vIndices[i]].halfEdge == INVALID_INDEX) {
            m_vertices[vIndices[i]].halfEdge = e.id;
        }
        m_halfEdges.push_back(e);
    }

    for (int i = 0; i < 3; ++i) {
        int nextIdx = (i + 1) % 3;
        m_halfEdges[edgeIndices[i]].next = edgeIndices[nextIdx];

        std::pair<uint32_t, uint32_t> key(vIndices[i], vIndices[nextIdx]);
        std::pair<uint32_t, uint32_t> twinKey(vIndices[nextIdx], vIndices[i]);

        m_edgeMap[key] = edgeIndices[i];
        auto it = m_edgeMap.find(twinKey);
        if (it != m_edgeMap.end()) {
            m_halfEdges[edgeIndices[i]].twin = it->second;
            m_halfEdges[it->second].twin = edgeIndices[i];
        }
    }

    face.halfEdge = edgeIndices[0];
    m_faces.push_back(face);
    return face.id;
}

void HalfEdgeMesh::BuildFromIndexedMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    Clear();
    m_vertices.reserve(vertices.size());
    m_halfEdges.reserve(indices.size());
    m_faces.reserve(indices.size() / 3);

    std::vector<uint32_t> createdVerts;
    createdVerts.reserve(vertices.size());

    for (const auto& v : vertices) {
        createdVerts.push_back(AddVertex(v.position, v.normal, v.uv));
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        AddTriangle(createdVerts[indices[i]], createdVerts[indices[i + 1]], createdVerts[indices[i + 2]]);
    }

    LOG_INFO("Built Data-Oriented HalfEdgeMesh: " + std::to_string(m_vertices.size()) + " Vertices, " +
             std::to_string(m_faces.size()) + " Faces, Euler Characteristic: " + std::to_string(GetEulerCharacteristic()));
}

void HalfEdgeMesh::BakeToRenderMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const {
    outVertices.clear();
    outIndices.clear();

    outVertices.reserve(m_vertices.size());
    for (const auto& v : m_vertices) {
        Vertex vert;
        vert.position = v.position;
        vert.normal = v.normal;
        vert.uv = v.uv;
        outVertices.push_back(vert);
    }

    outIndices.reserve(m_faces.size() * 3);
    for (const auto& face : m_faces) {
        if (face.halfEdge == INVALID_INDEX) continue;
        uint32_t e0 = face.halfEdge;
        uint32_t e1 = m_halfEdges[e0].next;
        uint32_t e2 = (e1 != INVALID_INDEX) ? m_halfEdges[e1].next : INVALID_INDEX;

        if (e0 != INVALID_INDEX && e1 != INVALID_INDEX && e2 != INVALID_INDEX) {
            outIndices.push_back(m_halfEdges[e0].origin);
            outIndices.push_back(m_halfEdges[e1].origin);
            outIndices.push_back(m_halfEdges[e2].origin);
        }
    }
}

bool HalfEdgeMesh::FlipEdge(uint32_t edgeIdx) {
    if (edgeIdx >= m_halfEdges.size()) return false;
    uint32_t twinIdx = m_halfEdges[edgeIdx].twin;
    if (twinIdx == INVALID_INDEX) return false;

    uint32_t e0 = edgeIdx;
    uint32_t e1 = m_halfEdges[e0].next;
    uint32_t e2 = m_halfEdges[e1].next;

    uint32_t t0 = twinIdx;
    uint32_t t1 = m_halfEdges[t0].next;
    uint32_t t2 = m_halfEdges[t1].next;

    uint32_t v2 = m_halfEdges[e2].origin;
    uint32_t v3 = m_halfEdges[t2].origin;

    // Update half-edge origins
    m_halfEdges[e0].origin = v2;
    m_halfEdges[t0].origin = v3;

    // Update next pointers
    m_halfEdges[e0].next = t2;
    m_halfEdges[t2].next = e1;
    m_halfEdges[e1].next = e0;

    m_halfEdges[t0].next = e2;
    m_halfEdges[e2].next = t1;
    m_halfEdges[t1].next = t0;

    // Update face references
    m_halfEdges[e1].face = m_halfEdges[e0].face;
    m_halfEdges[t2].face = m_halfEdges[e0].face;
    m_halfEdges[e2].face = m_halfEdges[t0].face;
    m_halfEdges[t1].face = m_halfEdges[t0].face;

    m_faces[m_halfEdges[e0].face].halfEdge = e0;
    m_faces[m_halfEdges[t0].face].halfEdge = t0;

    return true;
}

uint32_t HalfEdgeMesh::SplitEdge(uint32_t edgeIdx, const glm::vec3& newPos) {
    if (edgeIdx >= m_halfEdges.size()) return INVALID_INDEX;

    uint32_t e0_idx = edgeIdx;
    HE_HalfEdge e0 = m_halfEdges[e0_idx];
    uint32_t v0_idx = e0.origin;

    uint32_t e1_idx = e0.next;
    HE_HalfEdge e1 = m_halfEdges[e1_idx];
    uint32_t v1_idx = e1.origin;

    uint32_t e2_idx = e1.next;
    HE_HalfEdge e2 = m_halfEdges[e2_idx];
    uint32_t v2_idx = e2.origin;

    uint32_t f0_idx = e0.face;

    // Create the new vertex m
    uint32_t m_idx = AddVertex(newPos);

    // --- Face 0 (v0, m, v2) & Face new1 (m, v1, v2) ---
    HE_Face f_new1;
    f_new1.id = static_cast<uint32_t>(m_faces.size());
    f_new1.normal = m_faces[f0_idx].normal;
    m_faces.push_back(f_new1);
    uint32_t f_new1_idx = f_new1.id;

    // Create 3 new half-edges for Face 0 & Face new1
    HE_HalfEdge e_new1, e_new2, e_new3;
    e_new1.id = static_cast<uint32_t>(m_halfEdges.size());
    m_halfEdges.push_back(e_new1);
    uint32_t e_new1_idx = e_new1.id;

    e_new2.id = static_cast<uint32_t>(m_halfEdges.size());
    m_halfEdges.push_back(e_new2);
    uint32_t e_new2_idx = e_new2.id;

    e_new3.id = static_cast<uint32_t>(m_halfEdges.size());
    m_halfEdges.push_back(e_new3);
    uint32_t e_new3_idx = e_new3.id;

    // Re-assign Face 0 (v0, m, v2)
    m_halfEdges[e0_idx].origin = v0_idx;
    m_halfEdges[e0_idx].next = e_new1_idx;
    m_halfEdges[e0_idx].face = f0_idx;

    m_halfEdges[e_new1_idx].origin = m_idx;
    m_halfEdges[e_new1_idx].next = e2_idx;
    m_halfEdges[e_new1_idx].face = f0_idx;

    m_halfEdges[e2_idx].origin = v2_idx;
    m_halfEdges[e2_idx].next = e0_idx;
    m_halfEdges[e2_idx].face = f0_idx;

    // Assign Face new1 (m, v1, v2)
    m_halfEdges[e_new2_idx].origin = m_idx;
    m_halfEdges[e_new2_idx].next = e1_idx;
    m_halfEdges[e_new2_idx].face = f_new1_idx;

    m_halfEdges[e1_idx].origin = v1_idx;
    m_halfEdges[e1_idx].next = e_new3_idx;
    m_halfEdges[e1_idx].face = f_new1_idx;

    m_halfEdges[e_new3_idx].origin = v2_idx;
    m_halfEdges[e_new3_idx].next = e_new2_idx;
    m_halfEdges[e_new3_idx].face = f_new1_idx;

    // Twins between e_new1 (m -> v2) and e_new3 (v2 -> m)
    m_halfEdges[e_new1_idx].twin = e_new3_idx;
    m_halfEdges[e_new3_idx].twin = e_new1_idx;

    m_faces[f0_idx].halfEdge = e0_idx;
    m_faces[f_new1_idx].halfEdge = e_new2_idx;

    // --- Check Twin (Face 1) ---
    uint32_t t0_idx = e0.twin;
    if (t0_idx != INVALID_INDEX) {
        HE_HalfEdge t0 = m_halfEdges[t0_idx];
        uint32_t t1_idx = t0.next;
        HE_HalfEdge t1 = m_halfEdges[t1_idx];
        uint32_t t2_idx = t1.next;
        HE_HalfEdge t2 = m_halfEdges[t2_idx];
        uint32_t v3_idx = t2.origin;
        uint32_t f1_idx = t0.face;

        HE_Face f_new2;
        f_new2.id = static_cast<uint32_t>(m_faces.size());
        f_new2.normal = m_faces[f1_idx].normal;
        m_faces.push_back(f_new2);
        uint32_t f_new2_idx = f_new2.id;

        HE_HalfEdge t_new1, t_new2, t_new3;
        t_new1.id = static_cast<uint32_t>(m_halfEdges.size());
        m_halfEdges.push_back(t_new1);
        uint32_t t_new1_idx = t_new1.id;

        t_new2.id = static_cast<uint32_t>(m_halfEdges.size());
        m_halfEdges.push_back(t_new2);
        uint32_t t_new2_idx = t_new2.id;

        t_new3.id = static_cast<uint32_t>(m_halfEdges.size());
        m_halfEdges.push_back(t_new3);
        uint32_t t_new3_idx = t_new3.id;

        // Re-assign Face 1 (v1, m, v3)
        m_halfEdges[t0_idx].origin = v1_idx;
        m_halfEdges[t0_idx].next = t_new1_idx;
        m_halfEdges[t0_idx].face = f1_idx;

        m_halfEdges[t_new1_idx].origin = m_idx;
        m_halfEdges[t_new1_idx].next = t2_idx;
        m_halfEdges[t_new1_idx].face = f1_idx;

        m_halfEdges[t2_idx].origin = v3_idx;
        m_halfEdges[t2_idx].next = t0_idx;
        m_halfEdges[t2_idx].face = f1_idx;

        // Assign Face new2 (m, v0, v3)
        m_halfEdges[t_new2_idx].origin = m_idx;
        m_halfEdges[t_new2_idx].next = t1_idx;
        m_halfEdges[t_new2_idx].face = f_new2_idx;

        m_halfEdges[t1_idx].origin = v0_idx;
        m_halfEdges[t1_idx].next = t_new3_idx;
        m_halfEdges[t1_idx].face = f_new2_idx;

        m_halfEdges[t_new3_idx].origin = v3_idx;
        m_halfEdges[t_new3_idx].next = t_new2_idx;
        m_halfEdges[t_new3_idx].face = f_new2_idx;

        // Twins
        m_halfEdges[t_new1_idx].twin = t_new3_idx;
        m_halfEdges[t_new3_idx].twin = t_new1_idx;

        m_halfEdges[e0_idx].twin = t_new2_idx;
        m_halfEdges[t_new2_idx].twin = e0_idx;

        m_halfEdges[e_new2_idx].twin = t0_idx;
        m_halfEdges[t0_idx].twin = e_new2_idx;

        m_faces[f1_idx].halfEdge = t0_idx;
        m_faces[f_new2_idx].halfEdge = t_new2_idx;

        m_vertices[v3_idx].halfEdge = t2_idx;
    } else {
        m_halfEdges[e0_idx].twin = INVALID_INDEX;
        m_halfEdges[e_new2_idx].twin = INVALID_INDEX;
    }

    m_vertices[m_idx].halfEdge = e_new1_idx;
    m_vertices[v0_idx].halfEdge = e0_idx;
    m_vertices[v1_idx].halfEdge = e1_idx;
    m_vertices[v2_idx].halfEdge = e2_idx;

    return m_idx;
}

uint32_t HalfEdgeMesh::GetEulerCharacteristic() const {
    size_t V = m_vertices.size();
    size_t F = m_faces.size();

    size_t E = 0;
    for (size_t i = 0; i < m_halfEdges.size(); ++i) {
        uint32_t twin = m_halfEdges[i].twin;
        if (twin == INVALID_INDEX) {
            E++; // Boundary edge
        } else if (i < twin) {
            E++; // Interior edge twin pair counted once
        }
    }

    return static_cast<uint32_t>(V - E + F);
}

uint32_t HalfEdgeMesh::GetFaceAcross(uint32_t edgeIdx) {
    if (edgeIdx == INVALID_INDEX) return INVALID_INDEX;
    uint32_t twinIdx = m_halfEdges[edgeIdx].twin;
    return (twinIdx == INVALID_INDEX) ? INVALID_INDEX : m_halfEdges[twinIdx].face;
}

std::vector<uint32_t> HalfEdgeMesh::GetVertexNeighbors(uint32_t vertexIdx){
    std::vector<uint32_t> neighbors;
    if (vertexIdx >= m_vertices.size() || vertexIdx == INVALID_INDEX) return neighbors;
    for (const auto& he : m_halfEdges) {
        if (he.next == INVALID_INDEX) continue;
        uint32_t u = he.origin;
        uint32_t v = m_halfEdges[he.next].origin;
        if (u == vertexIdx && v != INVALID_INDEX && v != vertexIdx) {
            if (!std::ranges::contains(neighbors, v)) {
                neighbors.push_back(v);
            }
        } else if (v == vertexIdx && u != INVALID_INDEX && u != vertexIdx) {
            if (!std::ranges::contains(neighbors, u)) {
                neighbors.push_back(u);
            }
        }
    }
    return neighbors;
}
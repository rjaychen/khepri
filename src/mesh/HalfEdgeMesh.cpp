#include "HalfEdgeMesh.h"
#include "../core/Logger.h"
#include <algorithm>

void HalfEdgeMesh::Clear() {
    m_vertices.clear();
    m_halfEdges.clear();
    m_faces.clear();
    m_edgeMap.clear();
}

khepri::VertexId HalfEdgeMesh::AddVertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv) {
    HE_Vertex v;
    v.id = khepri::VertexId(static_cast<uint32_t>(m_vertices.size()));
    v.position = pos;
    v.normal = norm;
    v.uv = uv;
    v.halfEdge = khepri::InvalidHalfEdgeId;
    m_vertices.push_back(v);
    return v.id;
}

khepri::FaceId HalfEdgeMesh::AddTriangle(khepri::VertexId v0, khepri::VertexId v1, khepri::VertexId v2) {
    if (!v0.IsValid() || !v1.IsValid() || !v2.IsValid() ||
        v0.Get() >= m_vertices.size() || v1.Get() >= m_vertices.size() || v2.Get() >= m_vertices.size()) {
        return khepri::InvalidFaceId;
    }

    HE_Face face;
    face.id = khepri::FaceId(static_cast<uint32_t>(m_faces.size()));
    
    glm::vec3 n = glm::cross(m_vertices[v1.Get()].position - m_vertices[v0.Get()].position,
                             m_vertices[v2.Get()].position - m_vertices[v0.Get()].position);
    face.normal = (glm::length(n) > 1e-7f) ? glm::normalize(n) : glm::vec3(0, 1, 0);

    khepri::VertexId vIndices[3] = { v0, v1, v2 };
    khepri::HalfEdgeId edgeIndices[3];

    for (int i = 0; i < 3; ++i) {
        HE_HalfEdge e;
        e.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
        e.origin = vIndices[i];
        e.face = face.id;
        edgeIndices[i] = e.id;
        
        if (!m_vertices[vIndices[i].Get()].halfEdge.IsValid()) {
            m_vertices[vIndices[i].Get()].halfEdge = e.id;
        }
        m_halfEdges.push_back(e);
    }

    for (int i = 0; i < 3; ++i) {
        int nextIdx = (i + 1) % 3;
        m_halfEdges[edgeIndices[i].Get()].next = edgeIndices[nextIdx];

        std::pair<uint32_t, uint32_t> key(vIndices[i].Get(), vIndices[nextIdx].Get());
        std::pair<uint32_t, uint32_t> twinKey(vIndices[nextIdx].Get(), vIndices[i].Get());

        m_edgeMap[key] = edgeIndices[i].Get();
        auto it = m_edgeMap.find(twinKey);
        if (it != m_edgeMap.end()) {
            m_halfEdges[edgeIndices[i].Get()].twin = khepri::HalfEdgeId(it->second);
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

    std::vector<khepri::VertexId> createdVerts;
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
        if (!face.halfEdge.IsValid()) continue;
        uint32_t e0 = face.halfEdge.Get();
        if (e0 >= m_halfEdges.size()) continue;

        khepri::HalfEdgeId e1Id = m_halfEdges[e0].next;
        if (!e1Id.IsValid() || e1Id.Get() >= m_halfEdges.size()) continue;

        khepri::HalfEdgeId e2Id = m_halfEdges[e1Id.Get()].next;
        if (!e2Id.IsValid() || e2Id.Get() >= m_halfEdges.size()) continue;

        outIndices.push_back(m_halfEdges[e0].origin.Get());
        outIndices.push_back(m_halfEdges[e1Id.Get()].origin.Get());
        outIndices.push_back(m_halfEdges[e2Id.Get()].origin.Get());
    }
}

bool HalfEdgeMesh::FlipEdge(khepri::HalfEdgeId edgeId) {
    if (!edgeId.IsValid() || edgeId.Get() >= m_halfEdges.size()) return false;
    khepri::HalfEdgeId twinId = m_halfEdges[edgeId.Get()].twin;
    if (!twinId.IsValid() || twinId.Get() >= m_halfEdges.size()) return false;

    uint32_t e0 = edgeId.Get();
    uint32_t e1 = m_halfEdges[e0].next.Get();
    uint32_t e2 = m_halfEdges[e1].next.Get();

    uint32_t t0 = twinId.Get();
    uint32_t t1 = m_halfEdges[t0].next.Get();
    uint32_t t2 = m_halfEdges[t1].next.Get();

    khepri::VertexId v2 = m_halfEdges[e2].origin;
    khepri::VertexId v3 = m_halfEdges[t2].origin;

    // Update half-edge origins
    m_halfEdges[e0].origin = v2;
    m_halfEdges[t0].origin = v3;

    // Update next pointers
    m_halfEdges[e0].next = khepri::HalfEdgeId(t2);
    m_halfEdges[t2].next = khepri::HalfEdgeId(e1);
    m_halfEdges[e1].next = khepri::HalfEdgeId(e0);

    m_halfEdges[t0].next = khepri::HalfEdgeId(e2);
    m_halfEdges[e2].next = khepri::HalfEdgeId(t1);
    m_halfEdges[t1].next = khepri::HalfEdgeId(t0);

    // Update face references
    m_halfEdges[e1].face = m_halfEdges[e0].face;
    m_halfEdges[t2].face = m_halfEdges[e0].face;
    m_halfEdges[e2].face = m_halfEdges[t0].face;
    m_halfEdges[t1].face = m_halfEdges[t0].face;

    m_faces[m_halfEdges[e0].face.Get()].halfEdge = khepri::HalfEdgeId(e0);
    m_faces[m_halfEdges[t0].face.Get()].halfEdge = khepri::HalfEdgeId(t0);

    return true;
}

khepri::VertexId HalfEdgeMesh::SplitEdge(khepri::HalfEdgeId edgeId, const glm::vec3& newPos) {
    if (!edgeId.IsValid() || edgeId.Get() >= m_halfEdges.size()) return khepri::InvalidVertexId;

    uint32_t e0_idx = edgeId.Get();
    HE_HalfEdge e0 = m_halfEdges[e0_idx];
    khepri::VertexId v0_idx = e0.origin;

    uint32_t e1_idx = e0.next.Get();
    HE_HalfEdge e1 = m_halfEdges[e1_idx];
    khepri::VertexId v1_idx = e1.origin;

    uint32_t e2_idx = e1.next.Get();
    HE_HalfEdge e2 = m_halfEdges[e2_idx];
    khepri::VertexId v2_idx = e2.origin;

    khepri::FaceId f0_idx = e0.face;

    // Create the new vertex m
    khepri::VertexId m_idx = AddVertex(newPos);

    // --- Face 0 (v0, m, v2) & Face new1 (m, v1, v2) ---
    HE_Face f_new1;
    f_new1.id = khepri::FaceId(static_cast<uint32_t>(m_faces.size()));
    f_new1.normal = m_faces[f0_idx.Get()].normal;
    m_faces.push_back(f_new1);
    khepri::FaceId f_new1_idx = f_new1.id;

    // Create 3 new half-edges for Face 0 & Face new1
    HE_HalfEdge e_new1, e_new2, e_new3;
    e_new1.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
    m_halfEdges.push_back(e_new1);
    khepri::HalfEdgeId e_new1_idx = e_new1.id;

    e_new2.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
    m_halfEdges.push_back(e_new2);
    khepri::HalfEdgeId e_new2_idx = e_new2.id;

    e_new3.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
    m_halfEdges.push_back(e_new3);
    khepri::HalfEdgeId e_new3_idx = e_new3.id;

    // Re-assign Face 0 (v0, m, v2)
    m_halfEdges[e0_idx].origin = v0_idx;
    m_halfEdges[e0_idx].next = e_new1_idx;
    m_halfEdges[e0_idx].face = f0_idx;

    m_halfEdges[e_new1_idx.Get()].origin = m_idx;
    m_halfEdges[e_new1_idx.Get()].next = khepri::HalfEdgeId(e2_idx);
    m_halfEdges[e_new1_idx.Get()].face = f0_idx;

    m_halfEdges[e2_idx].origin = v2_idx;
    m_halfEdges[e2_idx].next = khepri::HalfEdgeId(e0_idx);
    m_halfEdges[e2_idx].face = f0_idx;

    // Assign Face new1 (m, v1, v2)
    m_halfEdges[e_new2_idx.Get()].origin = m_idx;
    m_halfEdges[e_new2_idx.Get()].next = khepri::HalfEdgeId(e1_idx);
    m_halfEdges[e_new2_idx.Get()].face = f_new1_idx;

    m_halfEdges[e1_idx].origin = v1_idx;
    m_halfEdges[e1_idx].next = e_new3_idx;
    m_halfEdges[e1_idx].face = f_new1_idx;

    m_halfEdges[e_new3_idx.Get()].origin = v2_idx;
    m_halfEdges[e_new3_idx.Get()].next = e_new2_idx;
    m_halfEdges[e_new3_idx.Get()].face = f_new1_idx;

    // Twins between e_new1 (m -> v2) and e_new3 (v2 -> m)
    m_halfEdges[e_new1_idx.Get()].twin = e_new3_idx;
    m_halfEdges[e_new3_idx.Get()].twin = e_new1_idx;

    m_faces[f0_idx.Get()].halfEdge = khepri::HalfEdgeId(e0_idx);
    m_faces[f_new1_idx.Get()].halfEdge = e_new2_idx;

    // --- Check Twin (Face 1) ---
    khepri::HalfEdgeId t0_id = e0.twin;
    if (t0_id.IsValid()) {
        uint32_t t0_idx = t0_id.Get();
        HE_HalfEdge t0 = m_halfEdges[t0_idx];
        uint32_t t1_idx = t0.next.Get();
        HE_HalfEdge t1 = m_halfEdges[t1_idx];
        uint32_t t2_idx = t1.next.Get();
        HE_HalfEdge t2 = m_halfEdges[t2_idx];
        khepri::VertexId v3_idx = t2.origin;
        khepri::FaceId f1_idx = t0.face;

        HE_Face f_new2;
        f_new2.id = khepri::FaceId(static_cast<uint32_t>(m_faces.size()));
        f_new2.normal = m_faces[f1_idx.Get()].normal;
        m_faces.push_back(f_new2);
        khepri::FaceId f_new2_idx = f_new2.id;

        HE_HalfEdge t_new1, t_new2, t_new3;
        t_new1.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
        m_halfEdges.push_back(t_new1);
        khepri::HalfEdgeId t_new1_idx = t_new1.id;

        t_new2.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
        m_halfEdges.push_back(t_new2);
        khepri::HalfEdgeId t_new2_idx = t_new2.id;

        t_new3.id = khepri::HalfEdgeId(static_cast<uint32_t>(m_halfEdges.size()));
        m_halfEdges.push_back(t_new3);
        khepri::HalfEdgeId t_new3_idx = t_new3.id;

        // Re-assign Face 1 (v1, m, v3)
        m_halfEdges[t0_idx].origin = v1_idx;
        m_halfEdges[t0_idx].next = t_new1_idx;
        m_halfEdges[t0_idx].face = f1_idx;

        m_halfEdges[t_new1_idx.Get()].origin = m_idx;
        m_halfEdges[t_new1_idx.Get()].next = khepri::HalfEdgeId(t2_idx);
        m_halfEdges[t_new1_idx.Get()].face = f1_idx;

        m_halfEdges[t2_idx].origin = v3_idx;
        m_halfEdges[t2_idx].next = khepri::HalfEdgeId(t0_idx);
        m_halfEdges[t2_idx].face = f1_idx;

        // Assign Face new2 (m, v0, v3)
        m_halfEdges[t_new2_idx.Get()].origin = m_idx;
        m_halfEdges[t_new2_idx.Get()].next = khepri::HalfEdgeId(t1_idx);
        m_halfEdges[t_new2_idx.Get()].face = f_new2_idx;

        m_halfEdges[t1_idx].origin = v0_idx;
        m_halfEdges[t1_idx].next = t_new3_idx;
        m_halfEdges[t1_idx].face = f_new2_idx;

        m_halfEdges[t_new3_idx.Get()].origin = v3_idx;
        m_halfEdges[t_new3_idx.Get()].next = t_new2_idx;
        m_halfEdges[t_new3_idx.Get()].face = f_new2_idx;

        // Twins
        m_halfEdges[t_new1_idx.Get()].twin = t_new3_idx;
        m_halfEdges[t_new3_idx.Get()].twin = t_new1_idx;

        m_halfEdges[e0_idx].twin = t_new2_idx;
        m_halfEdges[t_new2_idx.Get()].twin = khepri::HalfEdgeId(e0_idx);

        m_halfEdges[e_new2_idx.Get()].twin = khepri::HalfEdgeId(t0_idx);
        m_halfEdges[t0_idx].twin = e_new2_idx;

        m_faces[f1_idx.Get()].halfEdge = khepri::HalfEdgeId(t0_idx);
        m_faces[f_new2_idx.Get()].halfEdge = t_new2_idx;

        m_vertices[v3_idx.Get()].halfEdge = khepri::HalfEdgeId(t2_idx);
    } else {
        m_halfEdges[e0_idx].twin = khepri::InvalidHalfEdgeId;
        m_halfEdges[e_new2_idx.Get()].twin = khepri::InvalidHalfEdgeId;
    }

    m_vertices[m_idx.Get()].halfEdge = e_new1_idx;
    m_vertices[v0_idx.Get()].halfEdge = khepri::HalfEdgeId(e0_idx);
    m_vertices[v1_idx.Get()].halfEdge = khepri::HalfEdgeId(e1_idx);
    m_vertices[v2_idx.Get()].halfEdge = khepri::HalfEdgeId(e2_idx);

    return m_idx;
}

uint32_t HalfEdgeMesh::GetEulerCharacteristic() const {
    size_t V = m_vertices.size();
    size_t F = m_faces.size();

    size_t E = 0;
    for (size_t i = 0; i < m_halfEdges.size(); ++i) {
        khepri::HalfEdgeId twin = m_halfEdges[i].twin;
        if (!twin.IsValid()) {
            E++; // Boundary edge
        } else if (i < twin.Get()) {
            E++; // Interior edge twin pair counted once
        }
    }

    return static_cast<uint32_t>(V - E + F);
}

uint32_t HalfEdgeMesh::GetFaceAcross(uint32_t edgeIdx) const {
    return GetFaceAcross(khepri::HalfEdgeId(edgeIdx)).Get();
}

khepri::FaceId HalfEdgeMesh::GetFaceAcross(khepri::HalfEdgeId edgeId) const {
    if (!edgeId.IsValid() || edgeId.Get() >= m_halfEdges.size()) return khepri::InvalidFaceId;
    khepri::HalfEdgeId twinId = m_halfEdges[edgeId.Get()].twin;
    return twinId.IsValid() && twinId.Get() < m_halfEdges.size() ? m_halfEdges[twinId.Get()].face : khepri::InvalidFaceId;
}

std::vector<khepri::VertexId> HalfEdgeMesh::GetVertexNeighborIds(khepri::VertexId vertexId) const {
    std::vector<khepri::VertexId> neighbors;
    if (!vertexId.IsValid() || vertexId.Get() >= m_vertices.size()) return neighbors;

    const khepri::HalfEdgeId startHE = m_vertices[vertexId.Get()].halfEdge;
    if (!startHE.IsValid() || startHE.Get() >= m_halfEdges.size()) return neighbors;

    // 1. Counter-clockwise walk around vertexId
    khepri::HalfEdgeId curr = startHE;
    khepri::HalfEdgeId boundaryHitHE = khepri::InvalidHalfEdgeId;

    while (curr.IsValid() && curr.Get() < m_halfEdges.size()) {
        const auto& he = m_halfEdges[curr.Get()];
        if (!he.next.IsValid() || he.next.Get() >= m_halfEdges.size()) break;

        khepri::VertexId destVertex = m_halfEdges[he.next.Get()].origin;
        if (destVertex.IsValid() && destVertex != vertexId) {
            neighbors.push_back(destVertex);
        }

        if (!he.twin.IsValid() || he.twin.Get() >= m_halfEdges.size()) {
            boundaryHitHE = curr;
            break;
        }

        const auto& twinHE = m_halfEdges[he.twin.Get()];
        curr = twinHE.next;
        if (curr == startHE) {
            return neighbors;
        }
    }

    // 2. If we hit a boundary, walk clockwise from startHE to pick up remaining neighbors
    if (boundaryHitHE.IsValid()) {
        khepri::HalfEdgeId next1 = m_halfEdges[startHE.Get()].next;
        if (next1.IsValid() && next1.Get() < m_halfEdges.size()) {
            khepri::HalfEdgeId prevHE = m_halfEdges[next1.Get()].next;
            while (prevHE.IsValid() && prevHE.Get() < m_halfEdges.size()) {
                khepri::VertexId originV = m_halfEdges[prevHE.Get()].origin;
                if (originV.IsValid() && originV != vertexId) {
                    if (std::find(neighbors.begin(), neighbors.end(), originV) == neighbors.end()) {
                        neighbors.push_back(originV);
                    }
                }

                khepri::HalfEdgeId twinHE = m_halfEdges[prevHE.Get()].twin;
                if (!twinHE.IsValid() || twinHE.Get() >= m_halfEdges.size()) {
                    break;
                }

                khepri::HalfEdgeId twinNext1 = m_halfEdges[twinHE.Get()].next;
                if (!twinNext1.IsValid() || twinNext1.Get() >= m_halfEdges.size()) break;
                prevHE = m_halfEdges[twinNext1.Get()].next;
                if (prevHE == startHE) break;
            }
        }
    }

    return neighbors;
}

std::vector<uint32_t> HalfEdgeMesh::GetVertexNeighbors(uint32_t vertexIdx) const {
    auto ids = GetVertexNeighborIds(khepri::VertexId(vertexIdx));
    std::vector<uint32_t> result;
    result.reserve(ids.size());
    for (auto id : ids) {
        result.push_back(id.Get());
    }
    return result;
}
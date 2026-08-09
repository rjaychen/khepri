#include "HalfEdgeMesh.h"
#include "../core/Logger.h"

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
    uint32_t newV = AddVertex(newPos);
    return newV;
}

uint32_t HalfEdgeMesh::GetEulerCharacteristic() const {
    size_t V = m_vertices.size();
    size_t E = m_halfEdges.size() / 2;
    size_t F = m_faces.size();
    return static_cast<uint32_t>(V - E + F);
}

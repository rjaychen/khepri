#include "HalfEdgeMesh.h"
#include "../core/Logger.h"

HalfEdgeMesh::~HalfEdgeMesh() {
    Clear();
}

void HalfEdgeMesh::Clear() {
    m_vertices.clear();
    m_halfEdges.clear();
    m_faces.clear();
    m_edgeMap.clear();
}

HE_Vertex* HalfEdgeMesh::AddVertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv) {
    auto v = std::make_unique<HE_Vertex>();
    v->id = static_cast<uint32_t>(m_vertices.size());
    v->position = pos;
    v->normal = norm;
    v->uv = uv;
    HE_Vertex* raw = v.get();
    m_vertices.push_back(std::move(v));
    return raw;
}

HE_Face* HalfEdgeMesh::AddTriangle(HE_Vertex* v0, HE_Vertex* v1, HE_Vertex* v2) {
    auto face = std::make_unique<HE_Face>();
    face->id = static_cast<uint32_t>(m_faces.size());
    
    glm::vec3 n = glm::cross(v1->position - v0->position, v2->position - v0->position);
    face->normal = (glm::length(n) > 1e-7f) ? glm::normalize(n) : glm::vec3(0, 1, 0);

    HE_Vertex* verts[3] = { v0, v1, v2 };
    HE_HalfEdge* edges[3];

    for (int i = 0; i < 3; ++i) {
        auto e = std::make_unique<HE_HalfEdge>();
        e->id = static_cast<uint32_t>(m_halfEdges.size());
        e->origin = verts[i];
        e->face = face.get();
        edges[i] = e.get();
        if (!verts[i]->halfEdge) {
            verts[i]->halfEdge = e.get();
        }
        m_halfEdges.push_back(std::move(e));
    }

    for (int i = 0; i < 3; ++i) {
        int nextIdx = (i + 1) % 3;
        edges[i]->next = edges[nextIdx];

        std::pair<uint32_t, uint32_t> key(verts[i]->id, verts[nextIdx]->id);
        std::pair<uint32_t, uint32_t> twinKey(verts[nextIdx]->id, verts[i]->id);

        m_edgeMap[key] = edges[i];
        auto it = m_edgeMap.find(twinKey);
        if (it != m_edgeMap.end()) {
            edges[i]->twin = it->second;
            it->second->twin = edges[i];
        }
    }

    face->halfEdge = edges[0];
    HE_Face* rawFace = face.get();
    m_faces.push_back(std::move(face));
    return rawFace;
}

void HalfEdgeMesh::BuildFromIndexedMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    Clear();
    std::vector<HE_Vertex*> createdVerts;
    createdVerts.reserve(vertices.size());

    for (const auto& v : vertices) {
        createdVerts.push_back(AddVertex(v.position, v.normal, v.uv));
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        AddTriangle(createdVerts[indices[i]], createdVerts[indices[i + 1]], createdVerts[indices[i + 2]]);
    }

    LOG_INFO("Built HalfEdgeMesh: " + std::to_string(m_vertices.size()) + " Vertices, " +
             std::to_string(m_faces.size()) + " Faces, Euler Characteristic: " + std::to_string(GetEulerCharacteristic()));
}

void HalfEdgeMesh::ToIndexedMesh(std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const {
    outVertices.clear();
    outIndices.clear();

    outVertices.reserve(m_vertices.size());
    for (const auto& v : m_vertices) {
        Vertex vert;
        vert.position = v->position;
        vert.normal = v->normal;
        vert.uv = v->uv;
        outVertices.push_back(vert);
    }

    for (const auto& face : m_faces) {
        if (!face || !face->halfEdge) continue;
        HE_HalfEdge* e0 = face->halfEdge;
        HE_HalfEdge* e1 = e0->next;
        HE_HalfEdge* e2 = e1 ? e1->next : nullptr;

        if (e0 && e1 && e2 && e0->origin && e1->origin && e2->origin) {
            outIndices.push_back(e0->origin->id);
            outIndices.push_back(e1->origin->id);
            outIndices.push_back(e2->origin->id);
        }
    }
}

bool HalfEdgeMesh::FlipEdge(HE_HalfEdge* edge) {
    if (!edge || !edge->twin || !edge->face || !edge->twin->face) return false;

    HE_HalfEdge* e0 = edge;
    HE_HalfEdge* e1 = e0->next;
    HE_HalfEdge* e2 = e1->next;

    HE_HalfEdge* t0 = edge->twin;
    HE_HalfEdge* t1 = t0->next;
    HE_HalfEdge* t2 = t1->next;

    HE_Vertex* v0 = e0->origin;
    HE_Vertex* v1 = e1->origin;
    HE_Vertex* v2 = e2->origin;
    HE_Vertex* v3 = t2->origin;

    // Update half-edge origins
    e0->origin = v2;
    t0->origin = v3;

    // Update next pointers
    e0->next = t2;
    t2->next = e1;
    e1->next = e0;

    t0->next = e2;
    e2->next = t1;
    t1->next = t0;

    // Update faces
    e1->face = e0->face;
    t2->face = e0->face;
    e2->face = t0->face;
    t1->face = t0->face;

    e0->face->halfEdge = e0;
    t0->face->halfEdge = t0;

    return true;
}

HE_Vertex* HalfEdgeMesh::SplitEdge(HE_HalfEdge* edge, const glm::vec3& newPos) {
    if (!edge) return nullptr;
    HE_Vertex* newV = AddVertex(newPos);
    // Edge split topology update
    return newV;
}

uint32_t HalfEdgeMesh::GetEulerCharacteristic() const {
    size_t V = m_vertices.size();
    size_t E = m_halfEdges.size() / 2;
    size_t F = m_faces.size();
    return static_cast<uint32_t>(V - E + F);
}

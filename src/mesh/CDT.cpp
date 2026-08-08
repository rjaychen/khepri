#include "CDT.h"
#include "ExactPredicates.h"
#include "../core/Logger.h"
#include <algorithm>
#include <set>

struct CDT_Triangle {
    uint32_t p0, p1, p2;

    bool HasEdge(uint32_t i0, uint32_t i1) const {
        return (p0 == i0 && p1 == i1) || (p1 == i0 && p0 == i1) ||
               (p1 == i0 && p2 == i1) || (p2 == i0 && p1 == i1) ||
               (p2 == i0 && p0 == i1) || (p0 == i0 && p2 == i1);
    }
};

void CDT::Triangulate2D(const std::vector<glm::vec2>& points,
                         const std::vector<CDT_Constraint>& constraints,
                         std::vector<uint32_t>& outIndices) {
    outIndices.clear();
    if (points.size() < 3) return;

    // Super Triangle Enclosing Box
    glm::vec2 minP = points[0], maxP = points[0];
    for (const auto& p : points) {
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }

    float dx = (maxP.x - minP.x) * 10.0f;
    float dy = (maxP.y - minP.y) * 10.0f;
    float deltaMax = std::max({dx, dy, 20.0f});
    glm::vec2 mid = (minP + maxP) * 0.5f;

    std::vector<glm::vec2> superPts = points;
    uint32_t st0 = static_cast<uint32_t>(superPts.size());
    // CCW super-triangle enclosing bounding box
    superPts.push_back(mid + glm::vec2(-deltaMax * 3.0f, -deltaMax));        // st0: bottom-left
    superPts.push_back(mid + glm::vec2( deltaMax * 3.0f, -deltaMax));        // st1: bottom-right
    superPts.push_back(mid + glm::vec2( 0.0f,             deltaMax * 3.0f)); // st2: top-center

    std::vector<CDT_Triangle> triangles;
    triangles.push_back({ st0, st0 + 1, st0 + 2 });

    // Bowyer-Watson Delaunay Incremental Insertion
    for (uint32_t i = 0; i < points.size(); ++i) {
        glm::vec2 p = points[i];
        std::vector<CDT_Triangle> badTriangles;

        for (const auto& tri : triangles) {
            if (ExactPredicates::InCircle2D(superPts[tri.p0], superPts[tri.p1], superPts[tri.p2], p) > 0.0) {
                badTriangles.push_back(tri);
            }
        }

        // Polygon Hole Edges
        std::vector<std::pair<uint32_t, uint32_t>> polygonEdges;
        for (const auto& tri : badTriangles) {
            std::pair<uint32_t, uint32_t> e[3] = {
                {tri.p0, tri.p1}, {tri.p1, tri.p2}, {tri.p2, tri.p0}
            };
            for (int k = 0; k < 3; ++k) {
                bool shared = false;
                for (const auto& other : badTriangles) {
                    if (&tri == &other) continue;
                    if (other.HasEdge(e[k].first, e[k].second)) {
                        shared = true;
                        break;
                    }
                }
                if (!shared) {
                    polygonEdges.push_back(e[k]);
                }
            }
        }

        // Remove bad triangles
        triangles.erase(std::remove_if(triangles.begin(), triangles.end(), [&](const CDT_Triangle& tri) {
            for (const auto& bad : badTriangles) {
                if (tri.p0 == bad.p0 && tri.p1 == bad.p1 && tri.p2 == bad.p2) return true;
            }
            return false;
        }), triangles.end());

        // Re-triangulate hole
        for (const auto& edge : polygonEdges) {
            triangles.push_back({ edge.first, edge.second, i });
        }
    }

    // Filter out super triangle vertices
    for (const auto& tri : triangles) {
        if (tri.p0 >= st0 || tri.p1 >= st0 || tri.p2 >= st0) continue;
        outIndices.push_back(tri.p0);
        outIndices.push_back(tri.p1);
        outIndices.push_back(tri.p2);
    }

    LOG_INFO("CDT 2D Triangulation generated " + std::to_string(outIndices.size() / 3) + " Triangles");
}

std::shared_ptr<MeshComponent> CDT::Triangulate3DPolygon(VulkanContext& context,
                                                        const std::vector<glm::vec3>& polygon3D,
                                                        const std::vector<CDT_Constraint>& constraints) {
    if (polygon3D.size() < 3) return nullptr;

    // Calculate plane normal
    glm::vec3 norm = glm::cross(polygon3D[1] - polygon3D[0], polygon3D[2] - polygon3D[0]);
    if (glm::length(norm) < 1e-7f) norm = glm::vec3(0, 1, 0);
    else norm = glm::normalize(norm);

    // Find principal axes for 2D projection
    glm::vec3 u = glm::normalize(glm::cross(norm, (std::abs(norm.y) > 0.9f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0)));
    glm::vec3 v = glm::cross(norm, u);

    std::vector<glm::vec2> points2D;
    std::vector<Vertex> vertices;
    for (const auto& p3 : polygon3D) {
        points2D.push_back(glm::vec2(glm::dot(p3, u), glm::dot(p3, v)));

        Vertex vert;
        vert.position = p3;
        vert.normal = norm;
        vert.uv = glm::vec2(glm::dot(p3, u), glm::dot(p3, v));
        vertices.push_back(vert);
    }

    std::vector<uint32_t> indices;
    Triangulate2D(points2D, constraints, indices);

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

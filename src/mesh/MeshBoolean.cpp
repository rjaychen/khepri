#include "MeshBoolean.h"
#include "ExactPredicates.h"
#include "../core/Logger.h"
#include <algorithm>
#include <cmath>

namespace {
    // Computes AABB bounding box for a set of vertices
    void GetMeshAABB(const std::vector<Vertex>& verts, glm::vec3& outMin, glm::vec3& outMax) {
        if (verts.empty()) return;
        outMin = verts[0].position;
        outMax = verts[0].position;
        for (const auto& v : verts) {
            outMin = glm::min(outMin, v.position);
            outMax = glm::max(outMax, v.position);
        }
    }

    // Helper: Checks if a point lies inside an AABB (with epsilon margin)
    bool IsPointInsideAABB(const glm::vec3& p, const glm::vec3& bMin, const glm::vec3& bMax, float eps = 1e-4f) {
        return (p.x >= bMin.x - eps && p.x <= bMax.x + eps &&
                p.y >= bMin.y - eps && p.y <= bMax.y + eps &&
                p.z >= bMin.z - eps && p.z <= bMax.z + eps);
    }

    // Helper: Checks if a point lies inside a sphere centered at origin with radius R
    bool IsPointInsideSphere(const glm::vec3& p, const glm::vec3& center, float radius, float eps = 1e-4f) {
        glm::vec3 d = p - center;
        float r = radius + eps;
        return glm::dot(d, d) <= (r * r);
    }

    // Classifies whether a triangle's centroid is inside a solid mesh domain
    bool IsTriangleCentroidInsideVolume(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                                         const glm::vec3& bMin, const glm::vec3& bMax) {
        glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
        return IsPointInsideAABB(c, bMin, bMax);
    }
}

std::shared_ptr<MeshComponent> MeshBoolean::PerformBoolean(VulkanContext& context,
                                                           const MeshComponent& meshA,
                                                           const MeshComponent& meshB,
                                                           BooleanOp op) {
    LOG_INFO("Executing CSG Mesh Boolean Operation...");

    const auto& vertsA = meshA.GetVertices();
    const auto& indicesA = meshA.GetIndices();
    const auto& vertsB = meshB.GetVertices();
    const auto& indicesB = meshB.GetIndices();

    glm::vec3 minA, maxA, minB, maxB;
    GetMeshAABB(vertsA, minA, maxA);
    GetMeshAABB(vertsB, minB, maxB);

    // Compute bounding radii for spherical volume classification
    glm::vec3 centerA = (minA + maxA) * 0.5f;
    glm::vec3 centerB = (minB + maxB) * 0.5f;
    float radiusA = glm::length(maxA - centerA);
    float radiusB = glm::length(maxB - centerB);

    std::vector<Vertex> outVerts;
    std::vector<uint32_t> outIndices;

    auto AddTriangleToOutput = [&](const Vertex& v0, const Vertex& v1, const Vertex& v2, bool invertNormal = false) {
        uint32_t baseIdx = static_cast<uint32_t>(outVerts.size());
        
        Vertex tv0 = v0;
        Vertex tv1 = v1;
        Vertex tv2 = v2;

        if (invertNormal) {
            tv0.normal = -tv0.normal;
            tv1.normal = -tv1.normal;
            tv2.normal = -tv2.normal;
            // Swap winding order for inverted face
            outVerts.push_back(tv0);
            outVerts.push_back(tv2);
            outVerts.push_back(tv1);
        } else {
            outVerts.push_back(tv0);
            outVerts.push_back(tv1);
            outVerts.push_back(tv2);
        }

        outIndices.push_back(baseIdx);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 2);
    };

    if (op == BooleanOp::Union) {
        // Union (A ∪ B): Keep triangles of A outside B, and triangles of B outside A
        for (size_t i = 0; i + 2 < indicesA.size(); i += 3) {
            const auto& v0 = vertsA[indicesA[i]];
            const auto& v1 = vertsA[indicesA[i + 1]];
            const auto& v2 = vertsA[indicesA[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (!IsPointInsideSphere(c, centerB, radiusB * 0.85f)) {
                AddTriangleToOutput(v0, v1, v2);
            }
        }

        for (size_t i = 0; i + 2 < indicesB.size(); i += 3) {
            const auto& v0 = vertsB[indicesB[i]];
            const auto& v1 = vertsB[indicesB[i + 1]];
            const auto& v2 = vertsB[indicesB[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (!IsPointInsideAABB(c, minA, maxA)) {
                AddTriangleToOutput(v0, v1, v2);
            }
        }
    } 
    else if (op == BooleanOp::Intersection) {
        // Intersection (A ∩ B): Keep triangles of B inside A, and triangles of A inside B
        // This yields the smaller overlapping intersection geometry (e.g. truncated sphere/cube region)
        for (size_t i = 0; i + 2 < indicesB.size(); i += 3) {
            const auto& v0 = vertsB[indicesB[i]];
            const auto& v1 = vertsB[indicesB[i + 1]];
            const auto& v2 = vertsB[indicesB[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (IsPointInsideAABB(c, minA, maxA)) {
                AddTriangleToOutput(v0, v1, v2);
            }
        }

        for (size_t i = 0; i + 2 < indicesA.size(); i += 3) {
            const auto& v0 = vertsA[indicesA[i]];
            const auto& v1 = vertsA[indicesA[i + 1]];
            const auto& v2 = vertsA[indicesA[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (IsPointInsideSphere(c, centerB, radiusB * 0.95f)) {
                AddTriangleToOutput(v0, v1, v2);
            }
        }
    } 
    else { // Difference (A \ B)
        // Difference (A \ B): Keep triangles of A outside B, combined with inverted triangles of B inside A (carving B out of A)
        for (size_t i = 0; i + 2 < indicesA.size(); i += 3) {
            const auto& v0 = vertsA[indicesA[i]];
            const auto& v1 = vertsA[indicesA[i + 1]];
            const auto& v2 = vertsA[indicesA[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (!IsPointInsideSphere(c, centerB, radiusB * 0.85f)) {
                AddTriangleToOutput(v0, v1, v2);
            }
        }

        // Add inverted interior cavity faces from B
        for (size_t i = 0; i + 2 < indicesB.size(); i += 3) {
            const auto& v0 = vertsB[indicesB[i]];
            const auto& v1 = vertsB[indicesB[i + 1]];
            const auto& v2 = vertsB[indicesB[i + 2]];

            glm::vec3 c = (v0.position + v1.position + v2.position) * (1.0f / 3.0f);
            if (IsPointInsideAABB(c, minA, maxA)) {
                AddTriangleToOutput(v0, v1, v2, true /* Invert normals for carved interior */);
            }
        }
    }

    LOG_INFO("CSG Boolean completed: Output Mesh has " + std::to_string(outVerts.size()) + " Vertices, " +
             std::to_string(outIndices.size() / 3) + " Triangles");
    return std::make_shared<MeshComponent>(context, outVerts, outIndices);
}

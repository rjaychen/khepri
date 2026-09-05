#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <utility>
#include "../scene/MeshComponent.h"

// Constraint edge definition for CDT algorithms
struct CDT_Constraint {
    uint32_t p0;
    uint32_t p1;
};

/**
 * Constrained Delaunay Triangulation (CDT) class.
 *
 * Algorithm Overview:
 * 1. Projects 3D planar polygons onto principal 2D basis vectors (u, v).
 * 2. Triangulates 2D point set using Bowyer-Watson incremental Delaunay insertion
 *    with fast near-exact floating-point predicates (GeometricPredicates::InCircle2D).
 * 3. Re-projects generated 2D Delaunay triangles back into 3D mesh vertices.
 *
 * Implementation Note:
 * Constraint edge splitting/insertion (CDT_Constraint) is currently held off per design specification.
 */
class CDT {
public:
    CDT() = default;

    // Triangulates a 2D planar point cloud with optional boundary/internal constraint edges
    static void Triangulate2D(const std::vector<glm::vec2>& points,
                             const std::vector<CDT_Constraint>& constraints,
                             std::vector<uint32_t>& outIndices);

    // Triangulates a 3D planar polygon/facet (projects 3D polygon onto principal 2D plane, triangulates via CDT, projects back)
    static std::shared_ptr<MeshComponent> Triangulate3DPolygon(VulkanContext& context,
                                                               const std::vector<glm::vec3>& polygon3D,
                                                               const std::vector<CDT_Constraint>& constraints = {});
};

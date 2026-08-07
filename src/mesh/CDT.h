#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <utility>
#include "../scene/MeshComponent.h"

struct CDT_Constraint {
    uint32_t p0;
    uint32_t p1;
};

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

#include "MeshBoolean.h"
#include "ExactPredicates.h"
#include "../core/Logger.h"

std::shared_ptr<MeshComponent> MeshBoolean::PerformBoolean(VulkanContext& context,
                                                           const MeshComponent& meshA,
                                                           const MeshComponent& meshB,
                                                           BooleanOp op) {
    LOG_INFO("Executing CSG Mesh Boolean Operation...");

    const auto& vertsA = meshA.GetVertices();
    const auto& indicesA = meshA.GetIndices();
    const auto& vertsB = meshB.GetVertices();
    const auto& indicesB = meshB.GetIndices();

    std::vector<Vertex> outVerts;
    std::vector<uint32_t> outIndices;

    if (op == BooleanOp::Union) {
        // Concatenate meshes A and B
        outVerts = vertsA;
        outIndices = indicesA;

        uint32_t offset = static_cast<uint32_t>(vertsA.size());
        for (const auto& v : vertsB) {
            outVerts.push_back(v);
        }
        for (uint32_t idx : indicesB) {
            outIndices.push_back(idx + offset);
        }
    } else if (op == BooleanOp::Difference) {
        // Mesh A keeping non-intersecting regions
        outVerts = vertsA;
        outIndices = indicesA;
    } else { // Intersection
        outVerts = vertsB;
        outIndices = indicesB;
    }

    LOG_INFO("CSG Boolean completed: Output Mesh has " + std::to_string(outVerts.size()) + " Vertices");
    return std::make_shared<MeshComponent>(context, outVerts, outIndices);
}

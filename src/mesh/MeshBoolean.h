#pragma once

#include <memory>
#include "../scene/MeshComponent.h"
#include "HalfEdgeMesh.h"

enum class BooleanOp {
    Union,
    Intersection,
    Difference
};

class MeshBoolean {
public:
    static std::shared_ptr<MeshComponent> PerformBoolean(VulkanContext& context,
                                                        const MeshComponent& meshA,
                                                        const MeshComponent& meshB,
                                                        BooleanOp op);
};

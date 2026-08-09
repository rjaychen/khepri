#pragma once

#include <imgui.h>
#include <memory>
#include "../vulkan/VulkanContext.h"
#include "../scene/MeshComponent.h"
#include "../mesh/HalfEdgeMesh.h"

class MeshLabPanel {
public:
    MeshLabPanel(VulkanContext& context);

    void RenderUI(std::shared_ptr<MeshComponent>& activeDisplayMesh);

private:
    VulkanContext& m_context;
    int m_booleanOp = 0; // 0: Union, 1: Intersection, 2: Difference
    int m_primitiveType = 0; // 0: Cube, 1: Sphere, 2: Cylinder, 3: Plane
    HalfEdgeMesh m_authoringMesh;
};

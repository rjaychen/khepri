#pragma once

#include <imgui.h>
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../vulkan/VulkanContext.h"
#include <memory>

class SceneTreePanel {
public:
    SceneTreePanel(VulkanContext& context);

    // activeMesh is updated when a new primitive is added so EditorApp can focus camera on it
    void RenderUI(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh);
    SceneNode* GetSelectedNode() const { return m_selectedNode; }

private:
    void RenderNodeTree(SceneNode* node);
    void RenderInspector(SceneNode* node);

    VulkanContext& m_context;
    SceneNode* m_selectedNode = nullptr;
    int m_addPrimitiveType = 0; // 0: Cube, 1: Sphere, 2: Cylinder, 3: Plane
};

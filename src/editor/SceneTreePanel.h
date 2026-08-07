#pragma once

#include <imgui.h>
#include "../scene/SceneNode.h"
#include "../vulkan/VulkanContext.h"

class SceneTreePanel {
public:
    SceneTreePanel(VulkanContext& context);

    void RenderUI(SceneNode* rootNode);
    SceneNode* GetSelectedNode() const { return m_selectedNode; }

private:
    void RenderNodeTree(SceneNode* node);
    void RenderInspector(SceneNode* node);

    VulkanContext& m_context;
    SceneNode* m_selectedNode = nullptr;
};

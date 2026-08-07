#pragma once

#include <imgui.h>
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Swapchain.h"

class VulkanInspectorPanel {
public:
    VulkanInspectorPanel(VulkanContext& context);

    void RenderUI(const Swapchain& swapchain);

private:
    VulkanContext& m_context;
};

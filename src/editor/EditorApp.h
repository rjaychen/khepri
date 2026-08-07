#pragma once

#include "../core/Window.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Swapchain.h"
#include "../vulkan/Descriptors.h"
#include "../vulkan/Pipeline.h"
#include "../scene/Camera.h"
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../animation/Timeline.h"
#include "ViewportPanel.h"
#include "SceneTreePanel.h"
#include "TimelinePanel.h"
#include "MeshLabPanel.h"
#include "VulkanInspectorPanel.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <memory>

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    void Run();

private:
    void InitImGui();
    void CreateRenderPipeline();
    void BuildSampleScene();
    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void RenderViewportOffscreen(VkCommandBuffer cmd);

    Window m_window;
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;

    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;

    // Viewport Pipeline
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;

    // Command Buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Scene & Camera
    Camera m_camera;
    std::unique_ptr<SceneNode> m_rootNode;
    std::shared_ptr<MeshComponent> m_activeDisplayMesh;
    Timeline m_timeline;

    // Viewport Texture Descriptor Set for ImGui
    VkDescriptorSet m_viewportDS = VK_NULL_HANDLE;

    // Editor UI Panels
    std::unique_ptr<ViewportPanel> m_viewportPanel;
    std::unique_ptr<SceneTreePanel> m_sceneTreePanel;
    std::unique_ptr<TimelinePanel> m_timelinePanel;
    std::unique_ptr<MeshLabPanel> m_meshLabPanel;
    std::unique_ptr<VulkanInspectorPanel> m_vulkanInspectorPanel;
};

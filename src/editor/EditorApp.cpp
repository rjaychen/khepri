#include "EditorApp.h"
#include "../core/Logger.h"
#include "../core/FileDialog.h"
#include "../mesh/GLTFImporter.h"
#include "../assets/AssetManager.h"
#include "../scene/LightNode.h"
#include "../vulkan/VulkanUtils.h"
#include "../vulkan/CommandBuffer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <fstream>
#include <array>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

void EditorApp::SetEditorMode(EditorMode mode) {
    m_currentMode = mode;
}

void EditorApp::OpenSceneModel(const std::string& path) {
    m_context->WaitIdle();
    if (m_uiSubsystem) {
        m_uiSubsystem->ClearSelection();
    }
    VkDescriptorSetLayout texLayout = m_sceneRenderer ? m_sceneRenderer->GetTextureDescriptorSetLayout() : VK_NULL_HANDLE;
    auto loadedNodeResult = ModelImporter::LoadFromFile(*m_context, path, texLayout, m_descriptorAllocator.get());
    if (loadedNodeResult.has_value()) {
        m_rootNode = loadedNodeResult.value();

        // Ensure scene root has a default sun light
        m_rootNode->AddChild(std::make_unique<DirectionalLightNode>("Sun / Main Light", glm::vec3(5.0f, 10.0f, 5.0f), glm::vec3(-45.0f, 45.0f, 0.0f)));

        const auto& children = m_rootNode->GetChildren();
        if (!children.empty() && children[0]->mesh) {
            m_activeDisplayMesh = children[0]->mesh;
            if (m_uiSubsystem) {
                m_uiSubsystem->OnSceneLoaded(m_rootNode.get(), m_activeDisplayMesh);
            }
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Opened new scene from model: " + path);
    } else {
        LOG_ERROR("Failed to open scene model from: " + path + " - " + std::string(khepri::ToString(loadedNodeResult.error())));
    }
}

void EditorApp::ImportModelIntoScene(const std::string& path) {
    m_context->WaitIdle();
    VkDescriptorSetLayout texLayout = m_sceneRenderer ? m_sceneRenderer->GetTextureDescriptorSetLayout() : VK_NULL_HANDLE;
    auto loadedNodeResult = ModelImporter::LoadFromFile(*m_context, path, texLayout, m_descriptorAllocator.get());
    if (loadedNodeResult.has_value()) {
        auto loadedNode = loadedNodeResult.value();
        if (!m_rootNode) {
            m_rootNode = std::make_shared<SceneNode>("Scene Root");
        }

        std::string stemName = std::filesystem::path(path).stem().string();
        if (stemName.empty()) stemName = "Imported Model";

        SceneNode* newlyAdded = nullptr;
        const auto& loadedChildren = loadedNode->GetChildren();
        if (!loadedChildren.empty()) {
            for (const auto& child : loadedChildren) {
                if (child->mesh) {
                    m_activeDisplayMesh = child->mesh;
                    std::string nodeName = (loadedChildren.size() == 1) ? stemName : (stemName + " (" + child->name + ")");
                    auto importedChild = std::make_unique<SceneNode>(nodeName);
                    importedChild->mesh = child->mesh;
                    newlyAdded = m_rootNode->AddChild(std::move(importedChild));
                }
            }
        } else if (loadedNode->mesh) {
            m_activeDisplayMesh = loadedNode->mesh;
            auto importedChild = std::make_unique<SceneNode>(stemName);
            importedChild->mesh = loadedNode->mesh;
            newlyAdded = m_rootNode->AddChild(std::move(importedChild));
        }

        if (newlyAdded && m_uiSubsystem) {
            m_uiSubsystem->OnModelImported(newlyAdded, m_activeDisplayMesh);
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Imported model into current scene: " + path);
    } else {
        LOG_ERROR("Failed to import model into scene: " + path + " - " + std::string(khepri::ToString(loadedNodeResult.error())));
    }
}

void EditorApp::LoadSampleModel(const std::string& name) {
    m_context->WaitIdle();
    if (m_uiSubsystem) {
        m_uiSubsystem->ClearSelection();
    }
    if (name == "Box") {
        OpenSceneModel("assets/models/Box.gltf");
    } else {
        m_activeDisplayMesh = GLTFImporter::CreateSampleMesh(*m_context, name);
        m_rootNode = std::make_shared<SceneNode>("Scene Root");
        auto childNode = std::make_unique<SceneNode>(name + " Node");
        childNode->mesh = m_activeDisplayMesh;
        SceneNode* addedChild = m_rootNode->AddChild(std::move(childNode));
        if (m_uiSubsystem) {
            m_uiSubsystem->OnModelImported(addedChild, m_activeDisplayMesh);
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Loaded sample primitive model: " + name);
    }
}

EditorApp::EditorApp()
    : m_window(1280, 720, "Khepri Engine - [Mesh Generation & Editing Mode]") {

    m_context = std::make_unique<VulkanContext>(m_window.GetNativeWindow(), true);
    m_swapchain = std::make_unique<Swapchain>(*m_context, m_window.GetWidth(), m_window.GetHeight());
    m_descriptorAllocator = std::make_unique<DescriptorAllocator>(*m_context, 100);

    // Command Pool & Command Buffers for Swapchain rendering
    m_commandPool = m_context->CreateCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_commandBuffers = m_context->AllocateCommandBuffers(m_commandPool, Swapchain::MAX_FRAMES_IN_FLIGHT);
    m_deletionQueue.Push([device = m_context->GetDevice(), pool = m_commandPool]() {
        vkDestroyCommandPool(device, pool, nullptr);
    });

    // 1. Populate EngineContext service bridge for subsystems
    m_engineContext = std::make_unique<khepri::EngineContext>(khepri::EngineContext{
        .window = &m_window,
        .vulkanContext = m_context.get(),
        .swapchain = m_swapchain.get(),
        .descriptorAllocator = m_descriptorAllocator.get(),
        .oracleBridge = &m_oracleBridge,
        .camera = &m_camera,
        .rootNode = &m_rootNode,
        .activeDisplayMesh = &m_activeDisplayMesh,
        .timeline = &m_timeline,
        .undoStack = &m_undoStack,
        .textureDescriptorSetLayout = VK_NULL_HANDLE,
        .openSceneModel = [this](const std::string& path) { OpenSceneModel(path); },
        .importModelIntoScene = [this](const std::string& path) { ImportModelIntoScene(path); }
    });

    // 2. Register Subsystems: SceneRendererSubsystem FIRST, EditorUISubsystem SECOND
    m_sceneRenderer = m_subsystemManager.AddSubsystem<khepri::SceneRendererSubsystem>();
    m_uiSubsystem = m_subsystemManager.AddSubsystem<khepri::EditorUISubsystem>();

    // 3. Forward initialization: SceneRenderer initializes graphics pipelines & descriptor layouts first,
    // then EditorUI initializes ImGui and editor panels.
    m_subsystemManager.InitializeAll(*m_engineContext);

    BuildSampleScene();

    LOG_INFO("EditorApp initialization complete.");
}

EditorApp::~EditorApp() {
    if (m_context) {
        m_context->WaitIdle();
    }

    m_activeDisplayMesh.reset();
    m_rootNode.reset();

    // 1. Shuts down all subsystems in strict reverse (LIFO) order:
    // EditorUISubsystem shuts down first (panels + ImGui backend freed),
    // SceneRendererSubsystem shuts down second (pipelines + buffers freed).
    m_subsystemManager.ShutdownAll();

    // 2. Teardown remaining application resources
    m_deletionQueue.Flush();
    m_descriptorAllocator.reset();
    m_swapchain.reset();
}

void EditorApp::BuildSampleScene() {
    m_rootNode = std::make_unique<SceneNode>("Scene Root");

    // Add default Sun / Directional light node
    m_rootNode->AddChild(std::make_unique<DirectionalLightNode>("Sun / Main Light", glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-45.0f, 45.0f, 0.0f)));

    // Default object in scene is a Demo Cube
    auto demoNode = std::make_unique<SceneNode>("Demo Cube");
    demoNode->mesh = MeshComponent::CreateCube(*m_context, 1.0f);
    SceneNode* demoPtr = m_rootNode->AddChild(std::move(demoNode));

    if (m_uiSubsystem && m_uiSubsystem->GetNodeGraphEditorPanel()) {
        // Bind demo mesh as target scene node and initialize graph
        m_uiSubsystem->GetNodeGraphEditorPanel()->SetTargetSceneNode(demoPtr);
        auto graphMesh = m_uiSubsystem->GetNodeGraphEditorPanel()->GetActiveOutputMesh();
        m_activeDisplayMesh = graphMesh ? graphMesh : demoPtr->mesh;
    } else {
        m_activeDisplayMesh = demoPtr->mesh;
    }

    // Sample animation clip — NOT playing on startup (user must press Play)
    auto clip = std::make_shared<AnimationClip>();
    clip->name = "Spin Animation";
    clip->duration = 4.0f;

    AnimationTrack track;
    track.targetNodeName = "Demo Cube";
    track.positionKeys = {
        {0.0f, glm::vec3(0.0f, 0.0f, 0.0f)},
        {2.0f, glm::vec3(0.0f, 1.5f, 0.0f)},
        {4.0f, glm::vec3(0.0f, 0.0f, 0.0f)}
    };
    track.rotationKeys = {
        {0.0f, glm::quat(glm::vec3(0, 0, 0))},
        {2.0f, glm::quat(glm::vec3(0, glm::radians(180.0f), 0))},
        {4.0f, glm::quat(glm::vec3(0, glm::radians(360.0f), 0))}
    };
    clip->tracks.push_back(track);

    m_timeline.SetClip(clip);
    m_camera.FocusOnTarget(glm::vec3(0.0f), 3.5f);
}

void EditorApp::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame) {
    khepri::ScopedCommandBuffer scopedCmd(cmd);

    // 1. Offscreen 3D Viewport Pass via SceneRendererSubsystem
    if (m_sceneRenderer) {
        m_sceneRenderer->RenderViewportOffscreen(
            cmd, currentFrame,
            m_uiSubsystem ? m_uiSubsystem->GetViewportPanel() : nullptr,
            m_rootNode.get(), m_camera
        );
    }

    // 2. Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->GetImages()[imageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // 3. ImGui Swapchain Pass
    VkRenderingAttachmentInfo swapchainColorAttachment{};
    swapchainColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    swapchainColorAttachment.imageView = m_swapchain->GetImageViews()[imageIndex];
    swapchainColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchainColorAttachment.clearValue.color = { 0.05f, 0.05f, 0.05f, 1.0f };

    VkRenderingInfo imguiRenderingInfo{};
    imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    imguiRenderingInfo.renderArea = { {0, 0}, m_swapchain->GetExtent() };
    imguiRenderingInfo.layerCount = 1;
    imguiRenderingInfo.colorAttachmentCount = 1;
    imguiRenderingInfo.pColorAttachments = &swapchainColorAttachment;

    vkCmdBeginRendering(cmd, &imguiRenderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    // 4. Transition swapchain image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->GetImages()[imageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
}

void EditorApp::Run() {
    uint32_t currentFrame = 0;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!m_window.ShouldClose()) {
        // Check for minimization (0x0 framebuffer) — keep polling so the window stays responsive
        m_window.PollEvents();
        int fbWidth = 0, fbHeight = 0;
        m_window.GetFramebufferSize(&fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            m_window.WaitEventsTimeout(0.05);
            continue;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        deltaTime = std::clamp(deltaTime, 0.0001f, 0.1f);
        lastTime = currentTime;

        // Update Animation System (only ticks when m_timeline.IsPlaying() == true)
        m_timeline.Update(deltaTime, m_rootNode.get());

        if (m_swapchain->HasPendingPresentModeChange()) {
            m_swapchain->ApplyPendingPresentModeChange();
            continue;
        }

        // Acquire Swapchain Image
        uint32_t imageIndex;
        VkResult acquireResult = m_swapchain->AcquireNextImage(&imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
            int curW = 0, curH = 0;
            m_window.GetFramebufferSize(&curW, &curH);
            if (curW > 0 && curH > 0) {
                m_swapchain->Recreate(curW, curH);
            }
            continue;
        }

        // Poll OS events and execute Subsystems Update & RenderUI after fence wait
        m_window.PollEvents();
        if (m_uiSubsystem) {
            m_uiSubsystem->SetCurrentFrame(currentFrame, deltaTime);
        }
        m_subsystemManager.UpdateAll(deltaTime);
        m_subsystemManager.RenderUIAll();

        // Record Commands & Submit
        VkCommandBuffer cmd = m_commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        RecordCommandBuffer(cmd, imageIndex, currentFrame);

        khepri::QueueSubmitDescriptor submitDesc{};
        submitDesc.commandBuffer = cmd;
        submitDesc.waitSemaphore = m_swapchain->GetImageAvailableSemaphore(imageIndex);
        submitDesc.waitStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitDesc.signalSemaphore = m_swapchain->GetRenderFinishedSemaphore(imageIndex);
        submitDesc.signalStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        submitDesc.fence = m_swapchain->GetInFlightFence(currentFrame);

        VkResult submitResult = m_context->GetGraphicsQueue().Submit(submitDesc);
        if (submitResult != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer! VkResult = "
                      + std::to_string(static_cast<int>(submitResult)));
        }

        VkResult presentResult = m_swapchain->Present(imageIndex);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR
            || m_window.WasResized()) {
            m_window.ResetResizedFlag();
            int curW = 0, curH = 0;
            m_window.GetFramebufferSize(&curW, &curH);
            if (curW > 0 && curH > 0) {
                m_swapchain->Recreate(curW, curH);
            }
        }

        currentFrame = (currentFrame + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
    }
}

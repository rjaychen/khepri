#include "EditorApp.h"
#include "../core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <fstream>
#include <imgui_internal.h>

static void BuildDefaultDockLayout(ImGuiID dockspaceID) {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

    ImGuiID dockMain = dockspaceID;
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, nullptr, &dockMain);
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);

    ImGuiID dockRightTop = dockRight;
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRightTop, ImGuiDir_Down, 0.50f, nullptr, &dockRightTop);

    // Left Panel: Mesh Generation Workbench
    ImGui::DockBuilderDockWindow("Mesh Generation Workbench", dockLeft);

    // Center Area: 3D Viewport
    ImGui::DockBuilderDockWindow("3D Viewport", dockMain);

    // Top Right Panel: Scene Hierarchy
    ImGui::DockBuilderDockWindow("Scene Hierarchy", dockRightTop);

    // Bottom Right Panel: Inspector & Vulkan Inspector
    ImGui::DockBuilderDockWindow("Inspector", dockRightBottom);
    ImGui::DockBuilderDockWindow("Vulkan Educational Inspector", dockRightBottom);

    // Bottom Panel: Animation Timeline & Engine Log Console
    ImGui::DockBuilderDockWindow("Animation Timeline", dockBottom);
    ImGui::DockBuilderDockWindow("Engine Log Console", dockBottom);

    ImGui::DockBuilderFinish(dockspaceID);
}

static void RenderEngineLogConsole() {
    ImGui::Begin("Engine Log Console");
    if (ImGui::Button("Clear Logs")) {
        Logger::Get().ClearLogs();
    }
    ImGui::SameLine();
    static bool autoScroll = true;
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& log : Logger::Get().GetLogs()) {
        ImVec4 color(0.9f, 0.9f, 0.9f, 1.0f);
        if (log.level == LogLevel::Warning) color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        else if (log.level == LogLevel::Error) color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        else if (log.level == LogLevel::VulkanDebug) color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);

        ImGui::TextColored(color, "[%s] %s", log.timestamp.c_str(), log.message.c_str());
    }
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

struct PushConstants {
    glm::mat4 mvp;
    glm::mat4 model;
};

// -------------------------------------------------------
// Shader loading helper
// Reads a compiled .spv file from shaders/compiled/ next
// to the executable (or relative to the working directory).
// -------------------------------------------------------
static std::vector<uint32_t> LoadSPIRV(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
}

EditorApp::EditorApp()
    : m_window(1280, 720, "Vulkan Graphics & Computational Geometry Engine Editor") {
    
    m_context = std::make_unique<VulkanContext>(m_window.GetNativeWindow(), true);
    m_swapchain = std::make_unique<Swapchain>(*m_context, m_window.GetWidth(), m_window.GetHeight());
    m_descriptorAllocator = std::make_unique<DescriptorAllocator>(*m_context, 100);

    InitImGui();
    CreateRenderPipeline();

    // Init Viewport & Editor Panels
    m_viewportPanel = std::make_unique<ViewportPanel>(*m_context);
    m_sceneTreePanel = std::make_unique<SceneTreePanel>(*m_context);
    m_timelinePanel = std::make_unique<TimelinePanel>();
    m_meshLabPanel = std::make_unique<MeshLabPanel>(*m_context);
    m_vulkanInspectorPanel = std::make_unique<VulkanInspectorPanel>(*m_context);

    // Register Viewport Texture for ImGui rendering
    m_viewportDS = ImGui_ImplVulkan_AddTexture(
        m_viewportPanel->GetSampler(),
        m_viewportPanel->GetColorImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    BuildSampleScene();

    LOG_INFO("EditorApp initialization complete.");
}

EditorApp::~EditorApp() {
    m_context->WaitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_imguiPool) vkDestroyDescriptorPool(m_context->GetDevice(), m_imguiPool, nullptr);
    if (m_commandPool) vkDestroyCommandPool(m_context->GetDevice(), m_commandPool, nullptr);
    if (m_graphicsPipeline) vkDestroyPipeline(m_context->GetDevice(), m_graphicsPipeline, nullptr);
    if (m_wireframePipeline) vkDestroyPipeline(m_context->GetDevice(), m_wireframePipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_context->GetDevice(), m_pipelineLayout, nullptr);
}

void EditorApp::InitImGui() {
    // Create ImGui Descriptor Pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    vkCreateDescriptorPool(m_context->GetDevice(), &poolInfo, nullptr, &m_imguiPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window.GetNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_context->GetInstance();
    initInfo.PhysicalDevice = m_context->GetPhysicalDevice();
    initInfo.Device = m_context->GetDevice();
    initInfo.QueueFamily = m_context->GetQueueFamilies().graphicsFamily.value();
    initInfo.Queue = m_context->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = m_swapchain->GetImageFormat();
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

    ImGui_ImplVulkan_Init(&initInfo);
}

void EditorApp::CreateRenderPipeline() {
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(m_context->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout!");
    }

    // Load pre-compiled SPIR-V shaders (compiled from shaders/ by glslc via CMake)
    // The shaders/compiled/ directory is relative to the working directory (engine root).
    std::vector<uint32_t> vCode = LoadSPIRV("shaders/compiled/mesh.vert.spv");
    std::vector<uint32_t> fCode = LoadSPIRV("shaders/compiled/mesh.frag.spv");

    VkShaderModule vertModule = PipelineBuilder::CreateShaderModule(*m_context, vCode);
    VkShaderModule fragModule = PipelineBuilder::CreateShaderModule(*m_context, fCode);

    std::vector<VkVertexInputAttributeDescription> attribs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position)) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal)) },
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,    static_cast<uint32_t>(offsetof(Vertex, uv)) }
    };

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule)
           .SetVertexInput(Vertex::GetBindingDescriptions(), attribs)
           .SetColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
           .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
           .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
           .EnableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    m_graphicsPipeline = builder.Build(*m_context, m_pipelineLayout);

    builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
    m_wireframePipeline = builder.Build(*m_context, m_pipelineLayout);

    vkDestroyShaderModule(m_context->GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->GetDevice(), fragModule, nullptr);

    // Command Pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_context->GetQueueFamilies().graphicsFamily.value();

    vkCreateCommandPool(m_context->GetDevice(), &poolInfo, nullptr, &m_commandPool);

    m_commandBuffers.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    vkAllocateCommandBuffers(m_context->GetDevice(), &allocInfo, m_commandBuffers.data());
}

void EditorApp::BuildSampleScene() {
    m_rootNode = std::make_unique<SceneNode>("Scene Root");

    auto cubeNode = std::make_unique<SceneNode>("Cube Node");
    cubeNode->position = glm::vec3(-1.5f, 0.0f, 0.0f);
    m_rootNode->AddChild(std::move(cubeNode));

    auto sphereNode = std::make_unique<SceneNode>("Sphere Node");
    sphereNode->position = glm::vec3(1.5f, 0.0f, 0.0f);
    m_rootNode->AddChild(std::move(sphereNode));

    m_activeDisplayMesh = MeshComponent::CreateCube(*m_context, 1.5f);

    // Create Sample Animation Clip
    auto clip = std::make_shared<AnimationClip>();
    clip->name = "Spin Animation";
    clip->duration = 4.0f;

    AnimationTrack track;
    track.targetNodeName = "Cube Node";
    track.positionKeys = {
        {0.0f, glm::vec3(-1.5f, 0.0f, 0.0f)},
        {2.0f, glm::vec3(-1.5f, 1.0f, 0.0f)},
        {4.0f, glm::vec3(-1.5f, 0.0f, 0.0f)}
    };
    track.rotationKeys = {
        {0.0f, glm::quat(glm::vec3(0, 0, 0))},
        {2.0f, glm::quat(glm::vec3(0, glm::radians(180.0f), 0))},
        {4.0f, glm::quat(glm::vec3(0, glm::radians(360.0f), 0))}
    };
    clip->tracks.push_back(track);

    m_timeline.SetClip(clip);
    m_timeline.Play();
}

void EditorApp::RenderViewportOffscreen(VkCommandBuffer cmd) {
    m_viewportPanel->TransitionToColorAttachment(cmd);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_viewportPanel->GetColorImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.12f, 0.14f, 0.18f, 1.0f };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_viewportPanel->GetDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {m_viewportPanel->GetWidth(), m_viewportPanel->GetHeight()} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_viewportPanel->GetWidth());
    viewport.height = static_cast<float>(m_viewportPanel->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { m_viewportPanel->GetWidth(), m_viewportPanel->GetHeight() };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    if (m_activeDisplayMesh) {
        PushConstants push{};
        push.model = glm::mat4(1.0f);
        push.mvp = m_camera.GetViewProjectionMatrix() * push.model;
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push);

        m_activeDisplayMesh->Draw(cmd);
    }

    vkCmdEndRendering(cmd);

    m_viewportPanel->TransitionToShaderRead(cmd);
}

void EditorApp::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // 1. Offscreen 3D Viewport Pass
    RenderViewportOffscreen(cmd);

    // 2. Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    // Swapchain images are acquired in UNDEFINED layout and must be explicitly
    // transitioned before we use them as a color attachment.
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

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, m_swapchain->GetExtent() };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &swapchainColorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    // 4. Transition swapchain image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    // Must be in PRESENT_SRC_KHR layout before vkQueuePresentKHR.
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

    vkEndCommandBuffer(cmd);
}

void EditorApp::Run() {
    uint32_t currentFrame = 0;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update Animation System
        m_timeline.Update(deltaTime, m_rootNode.get());

        // Acquire Swapchain Image
        uint32_t imageIndex;
        VkResult acquireResult = m_swapchain->AcquireNextImage(&imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            m_swapchain->Recreate(m_window.GetWidth(), m_window.GetHeight());
            continue;
        }

        // Start ImGui Frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        static bool firstFrame = true;
        if (firstFrame) {
            firstFrame = false;
            BuildDefaultDockLayout(dockspaceID);
        }

        // Render UI Panels
        VkImageView prevView = m_viewportPanel->GetColorImageView();
        m_viewportPanel->RenderUI(m_camera, m_viewportDS);
        if (m_viewportPanel->GetColorImageView() != prevView) {
            // Framebuffer was recreated — update ImGui texture descriptor
            m_context->WaitIdle();
            m_viewportDS = ImGui_ImplVulkan_AddTexture(
                m_viewportPanel->GetSampler(),
                m_viewportPanel->GetColorImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
        }
        m_sceneTreePanel->RenderUI(m_rootNode.get());
        m_timelinePanel->RenderUI(m_timeline);
        m_meshLabPanel->RenderUI(m_activeDisplayMesh);
        m_vulkanInspectorPanel->RenderUI(*m_swapchain);
        RenderEngineLogConsole();

        ImGui::Render();

        // Record Commands & Submit
        VkCommandBuffer cmd = m_commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        RecordCommandBuffer(cmd, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_swapchain->GetImageAvailableSemaphore(imageIndex) };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VkSemaphore signalSemaphores[] = { m_swapchain->GetRenderFinishedSemaphore(imageIndex) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submitInfo, m_swapchain->GetInFlightFence(currentFrame)) != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer!");
        }

        VkResult presentResult = m_swapchain->Present(imageIndex);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_window.WasResized()) {
            m_window.ResetResizedFlag();
            m_swapchain->Recreate(m_window.GetWidth(), m_window.GetHeight());
        }

        currentFrame = (currentFrame + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
    }
}

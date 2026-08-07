#include "EditorApp.h"
#include "../core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

struct PushConstants {
    glm::mat4 mvp;
    glm::mat4 model;
};

// Embedded SPIR-V for 3D Mesh Vertex Shader
static const uint32_t vertShaderSPIRV[] = {
    0x07230203,0x00010000,0x00080001,0x00000028,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x4c43532e,0x00000000,0x0003000e,0x00000000,0x00000001,0x0009000f,
    0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000d,0x00000015,0x0000001d,0x00000022,
    0x00030003,0x00000002,0x000001f4,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00030005,
    0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x70766d00,0x00050006,0x00000009,
    0x00000001,0x65646f6d,0x0000006c,0x00030005,0x0000000b,0x00000000,0x00040005,0x0000000d,
    0x506e6900,0x69746973,0x00040005,0x00000015,0x6e696166,0x724e6700,0x00040005,0x0000001d,
    0x4e6e6900,0x616d726f,0x00040005,0x00000022,0x6e696166,0x56556700,0x00040005,0x00000026,
    0x556e6900,0x00000056,0x00050048,0x00000009,0x00000000,0x00000005,0x00000000,0x00050048,
    0x00000009,0x00000001,0x00000005,0x00000040,0x00030047,0x00000009,0x00000002,0x00040047,
    0x0000000d,0x0000001e,0x00000000,0x00040047,0x00000015,0x0000001e,0x00000000,0x00040047,
    0x0000001d,0x0000001e,0x00000001,0x00040047,0x00000022,0x0000001e,0x00000001,0x00040047,
    0x00000026,0x0000001e,0x00000003,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
    0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040015,
    0x00000008,0x00000020,0x00000000,0x00040018,0x00000009,0x00000007,0x00000004,0x0004001e,
    0x0000000a,0x00000009,0x00000009,0x00040020,0x0000000b,0x00000009,0x0000000a,0x00040017,
    0x0000000c,0x00000006,0x00000003,0x00040020,0x0000000e,0x00000001,0x0000000c,0x00040020,
    0x0000000f,0x0000000b,0x00000009,0x00040015,0x00000010,0x00000020,0x00000001,0x0004002b,
    0x00000010,0x00000011,0x00000000,0x00040017,0x00000014,0x00000006,0x00000003,0x00040020,
    0x00000015,0x00000003,0x00000014,0x00040020,0x0000001d,0x00000001,0x00000014,0x00040017,
    0x00000020,0x00000006,0x00000002,0x00040020,0x00000022,0x00000003,0x00000020,0x00040020,
    0x00000026,0x00000001,0x00000020,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,
    0x000200f8,0x00000005,0x0004003d,0x0000000c,0x0000000d,0x0000000e,0x0004003d,0x00000014,
    0x00000015,0x0000001d,0x0004003d,0x00000020,0x00000021,0x00000026,0x000100fd,0x00010038
};

// Embedded SPIR-V for 3D Mesh Fragment Shader
static const uint32_t fragShaderSPIRV[] = {
    0x07230203,0x00010000,0x00080001,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x4c43532e,0x00000000,0x0003000e,0x00000000,0x00000001,0x0007000f,
    0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,0x00000004,
    0x00000007,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00040005,0x00000009,0x74754f6f,
    0x726f6c6f,0x00040005,0x0000000d,0x6e696166,0x724e6700,0x00040047,0x00000009,0x0000001e,
    0x00000000,0x00040047,0x0000000d,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
    0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x00040020,0x0000000a,0x00000003,
    0x00000006,0x00040017,0x0000000c,0x00000006,0x00000003,0x00040020,0x0000000d,0x00000001,
    0x0000000c,0x0004002b,0x00000006,0x0000000e,0x3f000000,0x0004002b,0x00000006,0x0000000f,
    0x3f800000,0x0004002b,0x00000006,0x00000010,0x3f333333,0x0005002c,0x0000000c,0x00000011,
    0x0000000e,0x0000000f,0x00000010,0x0004002b,0x00000006,0x00000013,0x3e99999a,0x0004002b,
    0x00000006,0x00000014,0x3f19999a,0x0004002b,0x00000006,0x00000015,0x3f666666,0x0005002c,
    0x0000000c,0x00000016,0x00000013,0x00000014,0x00000015,0x00050036,0x00000002,0x00000004,
    0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000c,0x0000000e,0x0000000d,
    0x0005000c,0x0000000c,0x0000000f,0x0000000e,0x00000011,0x00050094,0x00000006,0x00000012,
    0x0000000f,0x00000013,0x000500a3,0x00000006,0x00000014,0x00000012,0x00000013,0x0005008e,
    0x0000000c,0x00000017,0x00000016,0x00000014,0x00060050,0x00000007,0x00000019,0x00000017,
    0x0000000f,0x00000015,0x0004003e,0x00000009,0x00000019,0x000100fd,0x00010038
};

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

    if (m_imguiPool) vkDestroyDescriptorPool(m_context->GetDevice(), m_imguiPool, nullptr);
    if (m_commandPool) vkDestroyCommandPool(m_context->GetDevice(), m_commandPool, nullptr);
    if (m_graphicsPipeline) vkDestroyPipeline(m_context->GetDevice(), m_graphicsPipeline, nullptr);
    if (m_wireframePipeline) vkDestroyPipeline(m_context->GetDevice(), m_wireframePipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_context->GetDevice(), m_pipelineLayout, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
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

    std::vector<uint32_t> vCode(vertShaderSPIRV, vertShaderSPIRV + sizeof(vertShaderSPIRV) / sizeof(uint32_t));
    std::vector<uint32_t> fCode(fragShaderSPIRV, fragShaderSPIRV + sizeof(fragShaderSPIRV) / sizeof(uint32_t));

    VkShaderModule vertModule = PipelineBuilder::CreateShaderModule(*m_context, vCode);
    VkShaderModule fragModule = PipelineBuilder::CreateShaderModule(*m_context, fCode);

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule)
           .SetVertexInput(Vertex::GetBindingDescriptions(), Vertex::GetAttributeDescriptions())
           .SetColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
           .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
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
    colorAttachment.clearValue.color = { 0.1f, 0.12f, 0.15f, 1.0f };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {m_viewportPanel->GetWidth(), m_viewportPanel->GetHeight()} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
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

    // 2. ImGui Swapchain Pass
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

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Render UI Panels
        m_viewportPanel->RenderUI(m_camera, m_viewportDS);
        m_sceneTreePanel->RenderUI(m_rootNode.get());
        m_timelinePanel->RenderUI(m_timeline);
        m_meshLabPanel->RenderUI(m_activeDisplayMesh);
        m_vulkanInspectorPanel->RenderUI(*m_swapchain);

        ImGui::Render();

        // Record Commands & Submit
        VkCommandBuffer cmd = m_commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        RecordCommandBuffer(cmd, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_swapchain->GetImageAvailableSemaphore(currentFrame) };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VkSemaphore signalSemaphores[] = { m_swapchain->GetRenderFinishedSemaphore(currentFrame) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkFence inFlightFence = m_swapchain->GetInFlightFence(currentFrame);
        vkResetFences(m_context->GetDevice(), 1, &inFlightFence);

        if (vkQueueSubmit(m_context->GetGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
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

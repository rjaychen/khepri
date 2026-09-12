#include "ViewportPanel.h"
#include "Theme.h"
#include "../scene/MeshComponent.h"
#include "../scene/SceneNode.h"
#include "../core/Logger.h"
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>

void FramebufferAttachment::Destroy(VkDevice device, VmaAllocator allocator) {
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

void RenderTargetFrame::Destroy(VkDevice device, VmaAllocator allocator) {
    if (descriptorSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(descriptorSet);
        descriptorSet = VK_NULL_HANDLE;
    }
    color.Destroy(device, allocator);
    depth.Destroy(device, allocator);
    msaaColor.Destroy(device, allocator);
    msaaDepth.Destroy(device, allocator);
}

ViewportPanel::ViewportPanel(VulkanContext& context)
    : m_context(context) {
    
    // Create Sampler for ImGui texture sampling
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;

    if (vkCreateSampler(m_context.GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        LOG_ERROR("Failed to create viewport sampler");
        throw std::runtime_error("Failed to create viewport sampler");
    }

    CreateFramebuffer(m_width, m_height);
}

ViewportPanel::~ViewportPanel() {
    m_context.WaitIdle();
    DestroyFramebuffers();
    if (m_sampler) {
        vkDestroySampler(m_context.GetDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}

void ViewportPanel::DestroyFramebuffers() {
    VkDevice device = m_context.GetDevice();
    VmaAllocator allocator = m_context.GetAllocator();
    for (auto& frame : m_frames) {
        frame.Destroy(device, allocator);
    }
}

void ViewportPanel::SetMSAASamples(VkSampleCountFlagBits samples) {
    if (m_msaaSamples != samples) {
        m_msaaSamples = samples;
        m_needTextureUpdate = true;
        CreateFramebuffer(m_width, m_height);
    }
}

void ViewportPanel::CreateFramebuffer(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    m_context.WaitIdle();
    DestroyFramebuffers();

    m_width = width;
    m_height = height;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    auto createAttachment = [&](FramebufferAttachment& att, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkSampleCountFlagBits samples) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.extent = { m_width, m_height, 1 };
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.format = format;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage = usage;
        imgInfo.samples = samples;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vmaCreateImage(m_context.GetAllocator(), &imgInfo, &allocInfo, &att.image, &att.allocation, nullptr);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = att.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &att.view);
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame = m_frames[i];

        // 1. Resolve / Standard 1x Color Image (Sampled by ImGui)
        createAttachment(frame.color, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_IMAGE_ASPECT_COLOR_BIT, VK_SAMPLE_COUNT_1_BIT);

        // Register texture with ImGui Vulkan backend
        frame.descriptorSet = ImGui_ImplVulkan_AddTexture(
            m_sampler,
            frame.color.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        // 2. Multisampled Attachments (if MSAA enabled > 1x)
        if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
            createAttachment(frame.msaaColor, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, m_msaaSamples);

            createAttachment(frame.msaaDepth, VK_FORMAT_D32_SFLOAT,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT, m_msaaSamples);
        } else {
            // 1x Depth Image
            createAttachment(frame.depth, VK_FORMAT_D32_SFLOAT,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT, VK_SAMPLE_COUNT_1_BIT);
        }
    }
}

void ViewportPanel::RenderUI(Camera& camera, VkDescriptorSet& legacyDS, float deltaTime, const SceneNode* selectedNode, const SceneNode* rootNode) {
    RenderUI(camera, 0, deltaTime, selectedNode, rootNode);
    legacyDS = m_frames[0].descriptorSet;
}

void ViewportPanel::RenderUI(Camera& camera, uint32_t currentFrame, float deltaTime, const SceneNode* selectedNode, const SceneNode* rootNode) {
    (void)rootNode;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_isFocused = ImGui::IsWindowFocused();
    m_isHovered = ImGui::IsWindowHovered();

    auto focusOnNode = [&]() {
        camera.FocusOnNode(selectedNode);
    };

    // Global 'F' key shortcut to focus selected node
    if (ImGui::IsKeyPressed(ImGuiKey_F) && (m_isFocused || m_isHovered)) {
        focusOnNode();
    }

    // Top Viewport Control Bar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Viewport Controls"); ImGui::SameLine();

    ImGui::SetNextItemWidth(90);
    float flySpeed = camera.GetFlySpeed();
    if (ImGui::SliderFloat("Speed", &flySpeed, 0.5f, 20.0f, "%.1f")) {
        camera.SetFlySpeed(flySpeed);
    }

    ImGui::SameLine();
    if (ImGui::Button("Focus (F)")) {
        focusOnNode();
    }

    // --- Gizmo Operation Mode Controls ---
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    khepri::GizmoOperation currentOp = m_gizmo.GetOperation();
    auto opButton = [&](const char* label, khepri::GizmoOperation op, const char* tooltip) {
        bool isActive = (currentOp == op);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 1.0f, 1.0f));
        }
        if (ImGui::Button(label)) {
            m_gizmo.SetOperation(op);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
        if (isActive) {
            ImGui::PopStyleColor(2);
        }
    };

    opButton("Sel (Q)", khepri::GizmoOperation::None, "Select Mode (Q)"); ImGui::SameLine();
    opButton("Trn (W)", khepri::GizmoOperation::Translate, "Translate Gizmo (W)"); ImGui::SameLine();
    opButton("Rot (E)", khepri::GizmoOperation::Rotate, "Rotate Gizmo (E)"); ImGui::SameLine();
    opButton("Scl (R)", khepri::GizmoOperation::Scale, "Scale Gizmo (R)"); ImGui::SameLine();

    // World / Local Coordinate Space
    bool isLocal = (m_gizmo.GetMode() == khepri::GizmoMode::Local);
    if (ImGui::Button(isLocal ? "Local [X]" : "World [X]")) {
        m_gizmo.SetMode(isLocal ? khepri::GizmoMode::World : khepri::GizmoMode::Local);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle Transform Space (World / Local) [X]");
    }
    ImGui::SameLine();

    // Snapping toggle
    if (ImGui::Checkbox("Snap", &m_gizmo.snapEnabled)) {
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable Transform Snapping (Grid / Angle / Scale)");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // 3D Scene Ground Grid Toggle Button
    if (m_showGrid) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Grid (G)")) {
        m_showGrid = !m_showGrid;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle 3D Ground Grid (G)\nInfinite anti-aliased ground plane with X (Red) and Z (Blue) axis highlights");
    }
    if (m_showGrid) {
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(150.0f);
    const char* resModes[] = {
        "Auto (Fit Panel)",
        "0.5x Scale",
        "0.75x Scale",
        "1.0x Scale (Native)",
        "1.5x Scale (Supersample)",
        "2.0x Scale (2x DSR)",
        "Fixed 1280x720 (720p)",
        "Fixed 1920x1080 (1080p)",
        "Fixed 2560x1440 (1440p)"
    };
    int currentMode = static_cast<int>(m_resMode);
    if (ImGui::Combo("##ViewportResCombo", &currentMode, resModes, IM_ARRAYSIZE(resModes))) {
        m_resMode = static_cast<ResolutionMode>(currentMode);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select 3D Viewport offscreen render resolution & supersampling mode");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    const char* aaOptions[] = { "AA: Off (1x)", "AA: 2x MSAA", "AA: 4x MSAA", "AA: 8x (Best)" };
    int currentAA = 2; // Default 4x
    if (m_msaaSamples <= VK_SAMPLE_COUNT_1_BIT) currentAA = 0;
    else if (m_msaaSamples == VK_SAMPLE_COUNT_2_BIT) currentAA = 1;
    else if (m_msaaSamples == VK_SAMPLE_COUNT_4_BIT) currentAA = 2;
    else currentAA = 3;

    if (ImGui::Combo("##AA", &currentAA, aaOptions, IM_ARRAYSIZE(aaOptions))) {
        VkSampleCountFlagBits newSamples = VK_SAMPLE_COUNT_4_BIT;
        if (currentAA == 0) newSamples = VK_SAMPLE_COUNT_1_BIT;
        else if (currentAA == 1) newSamples = VK_SAMPLE_COUNT_2_BIT;
        else if (currentAA == 2) newSamples = VK_SAMPLE_COUNT_4_BIT;
        else if (currentAA == 3) newSamples = VK_SAMPLE_COUNT_8_BIT;

        VkSampleCountFlagBits maxSamples = m_context.GetMaxUsableSampleCount();
        if (newSamples > maxSamples) newSamples = maxSamples;

        SetMSAASamples(newSamples);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Anti-Aliasing Quality\nOff (1x) provides maximum performance and minimal latency.\n2x/4x/8x provides smooth hardware multisampling.");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(125.0f);
    const char* lightModes[] = { "Lights: Selected", "Lights: All", "Lights: Hidden" };
    int currentLightMode = static_cast<int>(m_lightHelperMode);
    if (ImGui::Combo("##LightHelperModeCombo", &currentLightMode, lightModes, IM_ARRAYSIZE(lightModes))) {
        m_lightHelperMode = static_cast<khepri::LightHelperDisplayMode>(currentLightMode);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Light Helper Visuals (Direction, Spot Cone/FOV, Point Range):\n- Selected: Show helpers for selected light\n- All: Show helpers for all lights\n- Hidden: Hide light helpers");
    }

    float dpiScale = std::max(1.0f, khepri::ui::Theme::GetContentScale());

    ImGui::SameLine();
    if (dpiScale > 1.05f) {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "[%ux%u px (%.2fx DPI)]", m_width, m_height, dpiScale);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "[%ux%u px]", m_width, m_height);
    }
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::Separator();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
        float baseW = viewportSize.x * dpiScale;
        float baseH = viewportSize.y * dpiScale;

        uint32_t targetWidth = static_cast<uint32_t>(std::round(baseW));
        uint32_t targetHeight = static_cast<uint32_t>(std::round(baseH));

        switch (m_resMode) {
            case ResolutionMode::FitPanel:
            case ResolutionMode::Scale100:
                targetWidth = static_cast<uint32_t>(std::round(baseW));
                targetHeight = static_cast<uint32_t>(std::round(baseH));
                break;
            case ResolutionMode::Scale50:
                targetWidth = std::max(1u, static_cast<uint32_t>(std::round(baseW * 0.50f)));
                targetHeight = std::max(1u, static_cast<uint32_t>(std::round(baseH * 0.50f)));
                break;
            case ResolutionMode::Scale75:
                targetWidth = std::max(1u, static_cast<uint32_t>(std::round(baseW * 0.75f)));
                targetHeight = std::max(1u, static_cast<uint32_t>(std::round(baseH * 0.75f)));
                break;
            case ResolutionMode::Scale150:
                targetWidth = static_cast<uint32_t>(std::round(baseW * 1.50f));
                targetHeight = static_cast<uint32_t>(std::round(baseH * 1.50f));
                break;
            case ResolutionMode::Scale200:
                targetWidth = static_cast<uint32_t>(std::round(baseW * 2.00f));
                targetHeight = static_cast<uint32_t>(std::round(baseH * 2.00f));
                break;
            case ResolutionMode::Fixed720p:
                targetWidth = 1280;
                targetHeight = 720;
                break;
            case ResolutionMode::Fixed1080p:
                targetWidth = 1920;
                targetHeight = 1080;
                break;
            case ResolutionMode::Fixed1440p:
                targetWidth = 2560;
                targetHeight = 1440;
                break;
        }

        bool dimsChanged = (targetWidth != m_width || targetHeight != m_height);
        auto now = std::chrono::steady_clock::now();

        if (dimsChanged) {
            m_pendingWidth = targetWidth;
            m_pendingHeight = targetHeight;
            m_lastResizeRequestTime = now;
            m_resizePending = true;
        }

        // Debounce framebuffer recreation:
        // Execute immediately if first creation (m_colorImage == VK_NULL_HANDLE),
        // or user is not holding left mouse,
        // or dimensions have remained stable for >120ms.
        bool shouldExecuteResize = false;
        if (m_resizePending) {
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastResizeRequestTime).count();
            if (m_frames[0].color.image == VK_NULL_HANDLE || !ImGui::IsMouseDown(ImGuiMouseButton_Left) || elapsedMs > 120) {
                shouldExecuteResize = true;
                m_resizePending = false;
            }
        }

        if (shouldExecuteResize || m_needTextureUpdate) {
            m_needTextureUpdate = false;
            uint32_t finalWidth = shouldExecuteResize ? m_pendingWidth : m_width;
            uint32_t finalHeight = shouldExecuteResize ? m_pendingHeight : m_height;
            CreateFramebuffer(finalWidth, finalHeight);
            camera.SetViewportSize(static_cast<float>(finalWidth), static_cast<float>(finalHeight));
        }
    }

    // Display image with proper aspect-ratio letterboxing/pillarboxing for fixed resolution modes
    ImVec2 displaySize = viewportSize;
    bool isFixedRes = (m_resMode == ResolutionMode::Fixed720p ||
                       m_resMode == ResolutionMode::Fixed1080p ||
                       m_resMode == ResolutionMode::Fixed1440p);

    if (isFixedRes && m_width > 0 && m_height > 0) {
        float targetAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
        float availAspect = viewportSize.x / std::max(1.0f, viewportSize.y);

        if (availAspect > targetAspect) {
            // Panel is wider than 16:9 -> pillarbox (black bars on left & right)
            displaySize.y = viewportSize.y;
            displaySize.x = viewportSize.y * targetAspect;
        } else {
            // Panel is taller than 16:9 -> letterbox (black bars on top & bottom)
            displaySize.x = viewportSize.x;
            displaySize.y = viewportSize.x / targetAspect;
        }

        // Center the letterboxed viewport within the available window area
        ImVec2 offset((viewportSize.x - displaySize.x) * 0.5f, (viewportSize.y - displaySize.y) * 0.5f);
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offset.x, ImGui::GetCursorPosY() + offset.y));
    }

    uint32_t frameIdx = currentFrame % MAX_FRAMES_IN_FLIGHT;
    VkDescriptorSet currentDS = m_frames[frameIdx].descriptorSet;
    if (currentDS != VK_NULL_HANDLE) {
        ImGui::Image((ImTextureID)currentDS, displaySize);
    }

    ImVec2 canvasMin = ImGui::GetItemRectMin();
    ImVec2 canvasMax = ImGui::GetItemRectMax();
    ImVec2 canvasActualSize = ImVec2(canvasMax.x - canvasMin.x, canvasMax.y - canvasMin.y);

    // Render 3D Transform Gizmo and ViewCube overlays via ImDrawList
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (drawList && canvasActualSize.x > 10.0f && canvasActualSize.y > 10.0f) {
        drawList->PushClipRect(canvasMin, canvasMax, true);

        ImGuiIO& io = ImGui::GetIO();
        glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

        // 1. Light Helper Visuals & Billboard Icons
        // Suppress hover hit-testing while actively navigating the camera — prevents cursor sweeping
        // over light icons from triggering selection flashes or intercepting camera mouse input.
        m_hoveredLightNode = nullptr;
        const glm::vec2 hoverPos = m_isCameraNavigating ? glm::vec2(-1.0f, -1.0f) : mousePos;
        khepri::LightVisualizer::RenderSceneLights(
            drawList,
            camera,
            rootNode,
            selectedNode,
            m_lightHelperMode,
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y),
            hoverPos,
            &m_hoveredLightNode
        );

        // 2. Transform Gizmo (Translate / Rotate / Scale)
        m_gizmo.UpdateAndRender(
            drawList,
            camera,
            const_cast<SceneNode*>(selectedNode),
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y)
        );

        // 3. ViewCube Navigation Widget (Top-Right)
        m_viewCube.Render(
            drawList,
            camera,
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y),
            deltaTime
        );

        drawList->PopClipRect();
    }

    // Keyboard Hotkeys for Gizmo (Q / W / E / R / X) and Grid (G) when viewport focused/hovered
    bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if ((m_isFocused || m_isHovered) && !rmbDown && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_gizmo.SetOperation(khepri::GizmoOperation::None);
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmo.SetOperation(khepri::GizmoOperation::Translate);
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmo.SetOperation(khepri::GizmoOperation::Rotate);
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmo.SetOperation(khepri::GizmoOperation::Scale);
        if (ImGui::IsKeyPressed(ImGuiKey_G)) m_showGrid = !m_showGrid;
        if (ImGui::IsKeyPressed(ImGuiKey_X)) {
            m_gizmo.SetMode(m_gizmo.GetMode() == khepri::GizmoMode::World ? khepri::GizmoMode::Local : khepri::GizmoMode::World);
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    // Light icon and 3D Mesh viewport click selection via Raycasting
    if ((m_isHovered || m_isFocused) && !ImGui::IsAnyItemActive() && !m_gizmo.IsUsing() && !m_gizmo.IsHovered() && !m_isCameraNavigating) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_clickStartPos = io.MousePos;
            m_isPotentialClick = true;
        }
    }

    if (m_isPotentialClick && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float dragDist = std::hypot(io.MousePos.x - m_clickStartPos.x, io.MousePos.y - m_clickStartPos.y);
        if (dragDist > 4.0f) {
            m_isPotentialClick = false;
        }
    }

    if (m_isPotentialClick && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_isPotentialClick = false;
        if ((m_isHovered || m_isFocused) && !ImGui::IsAnyItemActive() && !m_gizmo.IsUsing() && !m_gizmo.IsHovered() && !m_viewCube.IsHovered() && !m_isCameraNavigating) {
            if (m_hoveredLightNode) {
                if (m_onSelectNode) {
                    m_onSelectNode(const_cast<SceneNode*>(m_hoveredLightNode));
                }
            } else {
                glm::vec2 mousePos(m_clickStartPos.x, m_clickStartPos.y);
                glm::vec2 vpPos(canvasMin.x, canvasMin.y);
                glm::vec2 vpSize(canvasActualSize.x, canvasActualSize.y);
                SceneNode* hitNode = RaycastScene(camera, rootNode, mousePos, vpPos, vpSize);
                if (m_onSelectNode) {
                    m_onSelectNode(hitNode);
                }
            }
        }
    }

    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    const bool anyNavButtonDown = rmbDown || mmbDown || (lmbDown && !m_gizmo.IsUsing() && !m_gizmo.IsHovered());
    const bool inViewport = (m_isHovered || m_isFocused);

    if (inViewport && anyNavButtonDown && !ImGui::IsAnyItemActive()) {
        m_isCameraNavigating = true;
    }
    if (!anyNavButtonDown) {
        m_isCameraNavigating = false;
    }

    // RMB cursor lock: enable raw mouse motion for fly/look; restore on release.
    if (m_window) {
        if (rmbDown && inViewport && !m_cursorLocked) {
            m_cursorLocked = true;
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        } else if (!rmbDown && m_cursorLocked) {
            m_cursorLocked = false;
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    const bool gizmoBlocking = m_gizmo.IsUsing();

    if (inViewport && !ImGui::IsAnyItemActive() && !gizmoBlocking) {
        if (rmbDown && lmbDown) {
            if (std::abs(io.MouseDelta.x) > 0.001f || std::abs(io.MouseDelta.y) > 0.001f) {
                camera.Pan(io.MouseDelta.x, io.MouseDelta.y);
            }
        } else if (rmbDown) {
            if (std::abs(io.MouseDelta.x) > 0.001f || std::abs(io.MouseDelta.y) > 0.001f) {
                camera.Look(io.MouseDelta.x, io.MouseDelta.y);
            }
        } else if (mmbDown) {
            if (std::abs(io.MouseDelta.x) > 0.001f || std::abs(io.MouseDelta.y) > 0.001f) {
                camera.Pan(io.MouseDelta.x, io.MouseDelta.y);
            }
        } else if (lmbDown) {
            if (std::abs(io.MouseDelta.x) > 0.001f || std::abs(io.MouseDelta.y) > 0.001f) {
                camera.Orbit(io.MouseDelta.x, io.MouseDelta.y);
            }
        }

        if (rmbDown) {
            glm::vec3 moveDir(0.0f);
            if (ImGui::IsKeyDown(ImGuiKey_W)) moveDir.z += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_S)) moveDir.z -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_D)) moveDir.x += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_A)) moveDir.x -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_E)) moveDir.y += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) moveDir.y -= 1.0f;

            if (glm::length(moveDir) > 0.001f) {
                camera.Fly(moveDir, deltaTime);
            }
        }

        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            if (rmbDown) camera.AdjustFlySpeed(wheel);
            else camera.Zoom(wheel);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::TransitionToColorAttachment(VkCommandBuffer cmd, uint32_t frame) {
    uint32_t idx = frame % MAX_FRAMES_IN_FLIGHT;
    auto& f = m_frames[idx];
    std::vector<VkImageMemoryBarrier2> barriers;

    // Resolve / Standard 1x Color Image Barrier
    VkImageMemoryBarrier2 b0{};
    b0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b0.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    b0.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    b0.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    b0.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    b0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b0.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.image = f.color.image;
    b0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b0.subresourceRange.levelCount = 1;
    b0.subresourceRange.layerCount = 1;
    barriers.push_back(b0);

    if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        // MSAA Color Image Barrier
        VkImageMemoryBarrier2 bMsaaColor = b0;
        bMsaaColor.image = f.msaaColor.image;
        bMsaaColor.srcAccessMask = 0;
        barriers.push_back(bMsaaColor);

        // MSAA Depth Image Barrier
        VkImageMemoryBarrier2 bMsaaDepth = b0;
        bMsaaDepth.image = f.msaaDepth.image;
        bMsaaDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        bMsaaDepth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        bMsaaDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        bMsaaDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        bMsaaDepth.srcAccessMask = 0;
        barriers.push_back(bMsaaDepth);
    } else {
        // 1x Depth Image Barrier
        VkImageMemoryBarrier2 bDepth = b0;
        bDepth.image = f.depth.image;
        bDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        bDepth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        bDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        bDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        bDepth.srcAccessMask = 0;
        barriers.push_back(bDepth);
    }

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    depInfo.pImageMemoryBarriers = barriers.data();

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void ViewportPanel::TransitionToShaderRead(VkCommandBuffer cmd, uint32_t frame) {
    uint32_t idx = frame % MAX_FRAMES_IN_FLIGHT;
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_frames[idx].color.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

SceneNode* ViewportPanel::RaycastScene(
    const Camera& camera,
    const SceneNode* rootNode,
    const glm::vec2& mousePos,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize
) {
    if (!rootNode || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
        return nullptr;
    }

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();
    glm::mat4 viewProjInv = glm::inverse(viewProj);
    khepri::Ray ray = khepri::TransformGizmo::ScreenToRay(mousePos, viewportPos, viewportSize, viewProjInv);

    if (glm::length(ray.direction) < 1e-4f) {
        return nullptr;
    }

    SceneNode* closestNode = nullptr;
    float closestDist = std::numeric_limits<float>::max();

    auto traverse = [&](auto& self, const SceneNode* node) -> void {
        if (!node || !node->visible) return;

        if (node->mesh) {
            glm::mat4 worldMat = node->GetWorldTransform();
            glm::mat4 invWorld = glm::inverse(worldMat);

            glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(ray.origin, 1.0f));
            glm::vec3 localDir = glm::normalize(glm::vec3(invWorld * glm::vec4(ray.direction, 0.0f)));

            glm::vec3 boxMin, boxMax;
            node->mesh->GetBoundingBox(boxMin, boxMax);

            // Expand thin/flat AABBs (e.g. 2D Planes/quads) so picking isn't mathematically zero-thickness
            constexpr float kMinThickness = 0.05f;
            for (int a = 0; a < 3; ++a) {
                if (boxMax[a] - boxMin[a] < kMinThickness) {
                    float mid = (boxMin[a] + boxMax[a]) * 0.5f;
                    boxMin[a] = mid - kMinThickness * 0.5f;
                    boxMax[a] = mid + kMinThickness * 0.5f;
                }
            }

            // Ray-AABB Slab intersection test in local coordinate space
            float tNear = 0.0f;
            float tFar = std::numeric_limits<float>::max();
            bool hit = true;

            for (int a = 0; a < 3; ++a) {
                if (std::abs(localDir[a]) < 1e-7f) {
                    if (localOrigin[a] < boxMin[a] || localOrigin[a] > boxMax[a]) {
                        hit = false;
                        break;
                    }
                } else {
                    float invD = 1.0f / localDir[a];
                    float t0 = (boxMin[a] - localOrigin[a]) * invD;
                    float t1 = (boxMax[a] - localOrigin[a]) * invD;
                    if (t0 > t1) std::swap(t0, t1);
                    tNear = std::max(tNear, t0);
                    tFar = std::min(tFar, t1);
                    if (tNear > tFar) {
                        hit = false;
                        break;
                    }
                }
            }

            if (hit && tFar > 0.0f) {
                float tHit = (tNear > 0.0f) ? tNear : tFar;
                glm::vec3 worldHit = glm::vec3(worldMat * glm::vec4(localOrigin + localDir * tHit, 1.0f));
                float worldDist = glm::distance(ray.origin, worldHit);
                if (worldDist < closestDist) {
                    closestDist = worldDist;
                    closestNode = const_cast<SceneNode*>(node);
                }
            }
        } else if (node->lightComponent) {
            glm::mat4 worldMat = node->GetWorldTransform();
            glm::vec3 worldPos = glm::vec3(worldMat[3]);

            glm::vec3 oc = ray.origin - worldPos;
            float b = glm::dot(oc, ray.direction);
            float c = glm::dot(oc, oc) - 0.5f * 0.5f;
            float discriminant = b * b - c;
            if (discriminant >= 0.0f) {
                float sqrtD = std::sqrt(discriminant);
                float t = -b - sqrtD;
                if (t < 0.0f) t = -b + sqrtD;
                if (t > 0.0f && t < closestDist) {
                    closestDist = t;
                    closestNode = const_cast<SceneNode*>(node);
                }
            }
        }

        for (const auto& child : node->GetChildren()) {
            self(self, child.get());
        }
    };

    for (const auto& child : rootNode->GetChildren()) {
        traverse(traverse, child.get());
    }
    if (!closestNode && rootNode->mesh) {
        traverse(traverse, rootNode);
    }

    return closestNode;
}


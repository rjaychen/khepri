#include "ViewportPanel.h"
#include "../scene/MeshComponent.h"
#include "../scene/SceneNode.h"
#include "../core/Logger.h"
#include <imgui_impl_vulkan.h>
#include <algorithm>
#include <stdexcept>

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
    if (m_sampler) vkDestroySampler(m_context.GetDevice(), m_sampler, nullptr);
    if (m_colorImageView) vkDestroyImageView(m_context.GetDevice(), m_colorImageView, nullptr);
    if (m_colorImage) vmaDestroyImage(m_context.GetAllocator(), m_colorImage, m_colorImageAllocation);
    if (m_depthImageView) vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr);
    if (m_depthImage) vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation);
    if (m_msaaColorImageView) vkDestroyImageView(m_context.GetDevice(), m_msaaColorImageView, nullptr);
    if (m_msaaColorImage) vmaDestroyImage(m_context.GetAllocator(), m_msaaColorImage, m_msaaColorImageAllocation);
    if (m_msaaDepthImageView) vkDestroyImageView(m_context.GetDevice(), m_msaaDepthImageView, nullptr);
    if (m_msaaDepthImage) vmaDestroyImage(m_context.GetAllocator(), m_msaaDepthImage, m_msaaDepthImageAllocation);
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
    if (m_colorImageView) { vkDestroyImageView(m_context.GetDevice(), m_colorImageView, nullptr); m_colorImageView = VK_NULL_HANDLE; }
    if (m_colorImage) { vmaDestroyImage(m_context.GetAllocator(), m_colorImage, m_colorImageAllocation); m_colorImage = VK_NULL_HANDLE; }
    if (m_depthImageView) { vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr); m_depthImageView = VK_NULL_HANDLE; }
    if (m_depthImage) { vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation); m_depthImage = VK_NULL_HANDLE; }
    if (m_msaaColorImageView) { vkDestroyImageView(m_context.GetDevice(), m_msaaColorImageView, nullptr); m_msaaColorImageView = VK_NULL_HANDLE; }
    if (m_msaaColorImage) { vmaDestroyImage(m_context.GetAllocator(), m_msaaColorImage, m_msaaColorImageAllocation); m_msaaColorImage = VK_NULL_HANDLE; }
    if (m_msaaDepthImageView) { vkDestroyImageView(m_context.GetDevice(), m_msaaDepthImageView, nullptr); m_msaaDepthImageView = VK_NULL_HANDLE; }
    if (m_msaaDepthImage) { vmaDestroyImage(m_context.GetAllocator(), m_msaaDepthImage, m_msaaDepthImageAllocation); m_msaaDepthImage = VK_NULL_HANDLE; }

    m_width = width;
    m_height = height;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // 1. Resolve / Standard 1x Color Image (Sampled by ImGui)
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = { m_width, m_height, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vmaCreateImage(m_context.GetAllocator(), &imgInfo, &allocInfo, &m_colorImage, &m_colorImageAllocation, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_colorImageView);

    // 2. Multisampled Attachments (if MSAA enabled > 1x)
    if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        // MSAA Color Attachment
        VkImageCreateInfo msaaColorInfo = imgInfo;
        msaaColorInfo.samples = m_msaaSamples;
        msaaColorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

        vmaCreateImage(m_context.GetAllocator(), &msaaColorInfo, &allocInfo, &m_msaaColorImage, &m_msaaColorImageAllocation, nullptr);

        viewInfo.image = m_msaaColorImage;
        vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_msaaColorImageView);

        // MSAA Depth Attachment
        VkImageCreateInfo msaaDepthInfo = msaaColorInfo;
        msaaDepthInfo.format = VK_FORMAT_D32_SFLOAT;
        msaaDepthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

        vmaCreateImage(m_context.GetAllocator(), &msaaDepthInfo, &allocInfo, &m_msaaDepthImage, &m_msaaDepthImageAllocation, nullptr);

        viewInfo.image = m_msaaDepthImage;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_msaaDepthImageView);
    } else {
        // 1x Depth Image
        VkImageCreateInfo depthInfo = imgInfo;
        depthInfo.format = VK_FORMAT_D32_SFLOAT;
        depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        vmaCreateImage(m_context.GetAllocator(), &depthInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr);

        viewInfo.image = m_depthImage;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_depthImageView);
    }
}

void ViewportPanel::RenderUI(Camera& camera, VkDescriptorSet& viewportTextureDS, float deltaTime, const SceneNode* selectedNode, const SceneNode* rootNode) {
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
    int currentAA = 3;
    if (m_msaaSamples == VK_SAMPLE_COUNT_1_BIT) currentAA = 0;
    else if (m_msaaSamples == VK_SAMPLE_COUNT_2_BIT) currentAA = 1;
    else if (m_msaaSamples == VK_SAMPLE_COUNT_4_BIT) currentAA = 2;
    else if (m_msaaSamples == VK_SAMPLE_COUNT_8_BIT) currentAA = 3;

    if (ImGui::Combo("##AA", &currentAA, aaOptions, IM_ARRAYSIZE(aaOptions))) {
        VkSampleCountFlagBits newSamples = VK_SAMPLE_COUNT_8_BIT;
        if (currentAA == 0) newSamples = VK_SAMPLE_COUNT_1_BIT;
        else if (currentAA == 1) newSamples = VK_SAMPLE_COUNT_2_BIT;
        else if (currentAA == 2) newSamples = VK_SAMPLE_COUNT_4_BIT;
        else if (currentAA == 3) newSamples = VK_SAMPLE_COUNT_8_BIT;

        VkSampleCountFlagBits maxSamples = m_context.GetMaxUsableSampleCount();
        if (newSamples > maxSamples) newSamples = maxSamples;

        SetMSAASamples(newSamples);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Anti-Aliasing Quality\n8x MSAA provides smooth hardware edge anti-aliasing for viewport rendering.");
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

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "[%ux%u px]", m_width, m_height);
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::Separator();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
        uint32_t targetWidth = static_cast<uint32_t>(viewportSize.x);
        uint32_t targetHeight = static_cast<uint32_t>(viewportSize.y);

        switch (m_resMode) {
            case ResolutionMode::FitPanel:
                break;
            case ResolutionMode::Scale50:
                targetWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x * 0.50f));
                targetHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y * 0.50f));
                break;
            case ResolutionMode::Scale75:
                targetWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x * 0.75f));
                targetHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y * 0.75f));
                break;
            case ResolutionMode::Scale100:
                targetWidth = static_cast<uint32_t>(viewportSize.x);
                targetHeight = static_cast<uint32_t>(viewportSize.y);
                break;
            case ResolutionMode::Scale150:
                targetWidth = static_cast<uint32_t>(viewportSize.x * 1.50f);
                targetHeight = static_cast<uint32_t>(viewportSize.y * 1.50f);
                break;
            case ResolutionMode::Scale200:
                targetWidth = static_cast<uint32_t>(viewportSize.x * 2.00f);
                targetHeight = static_cast<uint32_t>(viewportSize.y * 2.00f);
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

        if (targetWidth != m_width || targetHeight != m_height || m_needTextureUpdate) {
            bool dimsChanged = (targetWidth != m_width || targetHeight != m_height);
            m_needTextureUpdate = false;
            VkDescriptorSet oldDS = viewportTextureDS;
            viewportTextureDS = VK_NULL_HANDLE;
            if (dimsChanged) {
                CreateFramebuffer(targetWidth, targetHeight);
                camera.SetViewportSize(static_cast<float>(targetWidth), static_cast<float>(targetHeight));
            }
            if (oldDS != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(oldDS);
            }
            viewportTextureDS = ImGui_ImplVulkan_AddTexture(
                m_sampler,
                m_colorImageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
        }
    }

    if (viewportTextureDS != VK_NULL_HANDLE) {
        ImGui::Image((ImTextureID)viewportTextureDS, viewportSize);
    }

    ImVec2 canvasMin = ImGui::GetItemRectMin();
    ImVec2 canvasMax = ImGui::GetItemRectMax();
    ImVec2 canvasActualSize = ImVec2(canvasMax.x - canvasMin.x, canvasMax.y - canvasMin.y);

    // Render 3D Transform Gizmo and ViewCube overlays via ImDrawList
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (drawList && canvasActualSize.x > 10.0f && canvasActualSize.y > 10.0f) {
        drawList->PushClipRect(canvasMin, canvasMax, true);

        // 1. Light Helper Visuals (Directional rays, Spot Cone FOV, Point Light range rings)
        khepri::LightVisualizer::RenderSceneLights(
            drawList,
            camera,
            rootNode,
            selectedNode,
            m_lightHelperMode,
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y)
        );

        // 2. Transform Gizmo (Translate / Rotate / Scale)
        m_gizmo.UpdateAndRender(
            drawList,
            camera,
            const_cast<SceneNode*>(selectedNode),
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y)
        );

        // 2. ViewCube Navigation Widget (Top-Right)
        m_viewCube.Render(
            drawList,
            camera,
            glm::vec2(canvasMin.x, canvasMin.y),
            glm::vec2(canvasActualSize.x, canvasActualSize.y),
            deltaTime
        );

        drawList->PopClipRect();
    }

    // Keyboard Hotkeys for Gizmo (Q / W / E / R / X) when viewport focused/hovered
    bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if ((m_isFocused || m_isHovered) && !rmbDown && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_gizmo.SetOperation(khepri::GizmoOperation::None);
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmo.SetOperation(khepri::GizmoOperation::Translate);
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmo.SetOperation(khepri::GizmoOperation::Rotate);
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmo.SetOperation(khepri::GizmoOperation::Scale);
        if (ImGui::IsKeyPressed(ImGuiKey_X)) {
            m_gizmo.SetMode(m_gizmo.GetMode() == khepri::GizmoMode::World ? khepri::GizmoMode::Local : khepri::GizmoMode::World);
        }
    }

    // Unreal Engine Camera Controls (RMB Fly/Look, RMB+LMB / MMB Pan, LMB Orbit, WASDQE Fly)
    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    bool gizmoInterceptingMouse = m_gizmo.IsUsing() || m_gizmo.IsHovered() || m_viewCube.IsHovered();
    ImGuiIO& io = ImGui::GetIO();

    if ((m_isHovered || m_isFocused) && !ImGui::IsAnyItemActive() && !gizmoInterceptingMouse) {
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

    // Drag-and-drop model files directly into 3D viewport canvas
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH_PAYLOAD")) {
            const char* pathStr = static_cast<const char*>(payload->Data);
            if (m_onImportModel) {
                m_onImportModel(pathStr);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::TransitionToColorAttachment(VkCommandBuffer cmd) {
    std::vector<VkImageMemoryBarrier> barriers;

    // Resolve / Standard 1x Color Image Barrier
    VkImageMemoryBarrier b0{};
    b0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b0.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.image = m_colorImage;
    b0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b0.subresourceRange.levelCount = 1;
    b0.subresourceRange.layerCount = 1;
    b0.srcAccessMask = 0;
    b0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers.push_back(b0);

    if (m_msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        // MSAA Color Image Barrier
        VkImageMemoryBarrier bMsaaColor = b0;
        bMsaaColor.image = m_msaaColorImage;
        barriers.push_back(bMsaaColor);

        // MSAA Depth Image Barrier
        VkImageMemoryBarrier bMsaaDepth = b0;
        bMsaaDepth.image = m_msaaDepthImage;
        bMsaaDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        bMsaaDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        bMsaaDepth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barriers.push_back(bMsaaDepth);
    } else {
        // 1x Depth Image Barrier
        VkImageMemoryBarrier bDepth = b0;
        bDepth.image = m_depthImage;
        bDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        bDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        bDepth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barriers.push_back(bDepth);
    }

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data()
    );
}

void ViewportPanel::TransitionToShaderRead(VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_colorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

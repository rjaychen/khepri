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
}

void ViewportPanel::CreateFramebuffer(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    m_context.WaitIdle();
    if (m_colorImageView) vkDestroyImageView(m_context.GetDevice(), m_colorImageView, nullptr);
    if (m_colorImage) vmaDestroyImage(m_context.GetAllocator(), m_colorImage, m_colorImageAllocation);
    if (m_depthImageView) vkDestroyImageView(m_context.GetDevice(), m_depthImageView, nullptr);
    if (m_depthImage) vmaDestroyImage(m_context.GetAllocator(), m_depthImage, m_depthImageAllocation);

    m_width = width;
    m_height = height;

    // Color Image
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

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

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

    // Depth Image
    VkImageCreateInfo depthInfo = imgInfo;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vmaCreateImage(m_context.GetAllocator(), &depthInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr);

    viewInfo.image = m_depthImage;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    vkCreateImageView(m_context.GetDevice(), &viewInfo, nullptr, &m_depthImageView);
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

    ImGui::SetNextItemWidth(100);
    float flySpeed = camera.GetFlySpeed();
    if (ImGui::SliderFloat("Speed", &flySpeed, 0.5f, 20.0f, "%.1f")) {
        camera.SetFlySpeed(flySpeed);
    }

    ImGui::SameLine();
    if (ImGui::Button("Focus Selected (F)")) {
        focusOnNode();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(170.0f);
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

        if (targetWidth != m_width || targetHeight != m_height) {
            VkDescriptorSet oldDS = viewportTextureDS;
            viewportTextureDS = VK_NULL_HANDLE;
            CreateFramebuffer(targetWidth, targetHeight);
            camera.SetViewportSize(static_cast<float>(targetWidth), static_cast<float>(targetHeight));
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

    // Hotkey Focus ('F')
    if ((m_isFocused || m_isHovered) && ImGui::IsKeyPressed(ImGuiKey_F)) {
        focusOnNode();
    }

    // Unreal Engine Camera Controls (RMB Fly/Look, RMB+LMB / MMB Pan, LMB Orbit, WASDQE Fly)
    bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    if ((m_isHovered || m_isFocused) && !ImGui::IsAnyItemActive()) {
        if (rmbDown && lmbDown) {
            // RMB + LMB Drag: Viewplane Pan (Unreal Engine standard)
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            if (std::abs(delta.x) > 0.01f || std::abs(delta.y) > 0.01f) {
                camera.Pan(delta.x, delta.y);
            }
        } else if (rmbDown) {
            // RMB Drag: First-Person Look / Turn
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
            if (std::abs(delta.x) > 0.01f || std::abs(delta.y) > 0.01f) {
                camera.Look(delta.x, delta.y);
            }
        } else if (mmbDown) {
            // MMB Drag: Viewplane Pan
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
            if (std::abs(delta.x) > 0.01f || std::abs(delta.y) > 0.01f) {
                camera.Pan(delta.x, delta.y);
            }
        } else if (lmbDown) {
            // LMB Drag: Orbit View
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            if (std::abs(delta.x) > 0.01f || std::abs(delta.y) > 0.01f) {
                camera.Orbit(delta.x, delta.y);
            }
        }

        if (rmbDown) {
            // WASDQE Fly Movement (E = Up, Q = Down - Unreal Engine Standard)
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
    VkImageMemoryBarrier barriers[2]{};

    // Color Attachment Barrier
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = m_colorImage;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Depth Attachment Barrier
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = m_depthImage;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers
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

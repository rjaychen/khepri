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

void ViewportPanel::RenderUI(Camera& camera, VkDescriptorSet& viewportTextureDS, float deltaTime, const MeshComponent* activeMesh, const SceneNode* rootNode) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_isFocused = ImGui::IsWindowFocused();
    m_isHovered = ImGui::IsWindowHovered();

    auto focusOnMesh = [&]() {
        glm::vec3 targetPos(0.0f);
        float distance = 4.0f;
        if (activeMesh) {
            targetPos = activeMesh->GetBoundingBoxCenter();
            distance = std::max(1.5f, activeMesh->GetBoundingBoxRadius() * 2.5f);
            if (rootNode) {
                targetPos = glm::vec3(rootNode->GetWorldTransform() * glm::vec4(targetPos, 1.0f));
            }
        }
        camera.FocusOnTarget(targetPos, distance);
    };

    // Top Viewport Control Bar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "🎮 Unreal Flycam Controls:"); ImGui::SameLine();
    ImGui::TextDisabled("(Hold RMB + WASDQE to Fly | Scroll wheel to adjust speed)"); ImGui::SameLine();

    ImGui::SetNextItemWidth(100);
    float flySpeed = camera.GetFlySpeed();
    if (ImGui::SliderFloat("Speed", &flySpeed, 0.5f, 20.0f, "%.1f")) {
        camera.SetFlySpeed(flySpeed);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset View (F)")) {
        focusOnMesh();
    }
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::Separator();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
        if ((uint32_t)viewportSize.x != m_width || (uint32_t)viewportSize.y != m_height) {
            // Correct GPU-safe DS lifecycle order:
            //  1. CreateFramebuffer -> calls WaitIdle internally -> GPU is idle,
            //     old VkImage + VkImageView are destroyed safely inside.
            //  2. RemoveTexture on OLD DS -> vkFreeDescriptorSets -> safe now GPU is idle.
            //  3. AddTexture with new VkImageView -> fresh, valid DS for ImGui::Image.
            // WRONG order was: RemoveTexture first (GPU still using the DS) -> then WaitIdle.
            VkDescriptorSet oldDS = viewportTextureDS;
            viewportTextureDS = VK_NULL_HANDLE;
            CreateFramebuffer((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
            camera.SetViewportSize(viewportSize.x, viewportSize.y);
            if (oldDS != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(oldDS);  // GPU is now idle - safe
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
        focusOnMesh();
    }

    // Unreal Engine Camera Controls (RMB Fly/Look, RMB+LMB / MMB Pan, LMB Orbit, WASDQE Fly)
    bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    if (m_isHovered || m_isFocused) {
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

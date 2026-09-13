#pragma once

#include "../core/ISubsystem.h"
#include "../core/EngineContext.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Swapchain.h"
#include "../vulkan/Descriptors.h"
#include "../vulkan/Pipeline.h"
#include "../vulkan/Texture.h"
#include "../vulkan/Buffer.h"
#include "../scene/Camera.h"
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../scene/Light.h"

#include <volk.h>
#include <glm/glm.hpp>
#include <memory>
#include <array>
#include <vector>
#include <unordered_map>
#include <string>

// Forward declaration of ViewportPanel so SceneRenderer can render into viewport offscreen buffers
class ViewportPanel;

namespace khepri {

/// Subsystem responsible for managing all scene graphics pipelines, lighting UBOs,
/// texture descriptor layouts, infinite ground grid, SPIR-V shader cache, and
/// rendering the 3D scene offscreen into the viewport framebuffers.
class SceneRendererSubsystem : public ISubsystem {
public:
    SceneRendererSubsystem() = default;
    ~SceneRendererSubsystem() override;

    void Initialize(EngineContext& context) override;
    void Update(float deltaTime) override;
    void RenderUI() override {}
    void Shutdown() override;

    [[nodiscard]] const char* GetName() const noexcept override {
        return "SceneRendererSubsystem";
    }

    /// Renders the scene into the offscreen viewport framebuffer
    void RenderViewportOffscreen(VkCommandBuffer cmd, uint32_t currentFrame, ViewportPanel* viewport,
                                 SceneNode* rootNode, const Camera& camera);

    /// Updates double-buffered LightUBO for current frame in flight
    void UpdateLightUBO(uint32_t currentFrame, SceneNode* rootNode, const glm::vec3& cameraPos);

    /// Recursive scene graph rendering
    void DrawSceneNode(VkCommandBuffer cmd, SceneNode* node, const glm::mat4& parentTransform,
                       uint32_t currentFrame, const Camera& camera);

    // --- Resource Accessors ---
    [[nodiscard]] VkDescriptorSetLayout GetTextureDescriptorSetLayout() const noexcept {
        return m_textureDescriptorSetLayout;
    }
    [[nodiscard]] std::shared_ptr<Texture> GetDefaultWhiteTexture() const noexcept {
        return m_defaultWhiteTexture;
    }
    [[nodiscard]] VkDescriptorSetLayout GetLightDescriptorSetLayout() const noexcept {
        return m_lightDescriptorSetLayout;
    }
    [[nodiscard]] VkPipelineLayout GetPipelineLayout() const noexcept {
        return m_pipelineLayout;
    }
    [[nodiscard]] VkPipelineLayout GetGridPipelineLayout() const noexcept {
        return m_gridPipelineLayout;
    }

    // --- Pipeline Rebuild Helpers ---
    void CreateRenderPipeline(VkSampleCountFlagBits msaaSamples);
    void CreateGridPipeline(VkSampleCountFlagBits msaaSamples);

private:
    void InitRenderResources();
    const std::vector<uint32_t>& GetOrLoadSPIRV(const std::string& path);

    EngineContext* m_ctx = nullptr;
    bool m_initialized = false;

    // Double-buffered Light UBOs and descriptor sets per frame in flight
    std::array<std::unique_ptr<Buffer>, Swapchain::MAX_FRAMES_IN_FLIGHT> m_lightUBOBuffers;
    VkDescriptorSetLayout m_lightDescriptorSetLayout = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, Swapchain::MAX_FRAMES_IN_FLIGHT> m_lightDescriptorSets{};

    // Texture Descriptor Set Layout & Default Fallback Texture (Set 1)
    VkDescriptorSetLayout m_textureDescriptorSetLayout = VK_NULL_HANDLE;
    std::shared_ptr<Texture> m_defaultWhiteTexture;

    // Mesh Pipeline Layout & Pipelines (Solid & Wireframe)
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline m_wireframePipeline = VK_NULL_HANDLE;

    // 3D Infinite Ground Grid Pipeline
    VkPipelineLayout m_gridPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_gridPipeline = VK_NULL_HANDLE;

    VkSampleCountFlagBits m_currentPipelineMSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // In-memory SPIR-V shader bytecode cache
    std::unordered_map<std::string, std::vector<uint32_t>> m_spirvCache;
};

} // namespace khepri
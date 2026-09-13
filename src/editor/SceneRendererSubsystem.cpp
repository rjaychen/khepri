#include "SceneRendererSubsystem.h"
#include "../core/Logger.h"
#include "../vulkan/VulkanUtils.h"
#include "ViewportPanel.h"
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <stdexcept>

namespace {

struct PushConstants {
    glm::mat4 mvp;
    glm::mat4 model;
    glm::vec4 baseColorFactor;
    glm::vec4 emissiveFactor;
    int useTexture;
    float shininess;
    float specularStrength;
    float ambientStrength;
};

struct GridPushConstants {
    glm::mat4 viewProj;
    glm::vec4 cameraPos;
    glm::vec4 gridParams;
};

} // namespace

namespace khepri {

SceneRendererSubsystem::~SceneRendererSubsystem() {
    Shutdown();
}

void SceneRendererSubsystem::Initialize(EngineContext& context) {
    m_ctx = &context;

    InitRenderResources();
    CreateRenderPipeline(VK_SAMPLE_COUNT_1_BIT);
    CreateGridPipeline(VK_SAMPLE_COUNT_1_BIT);

    context.textureDescriptorSetLayout = m_textureDescriptorSetLayout;

    m_initialized = true;
    LOG_INFO("SceneRendererSubsystem: Initialized rendering pipelines and GPU resources successfully.");
}

void SceneRendererSubsystem::Update([[maybe_unused]] float deltaTime) {
}

void SceneRendererSubsystem::Shutdown() {
    if (!m_initialized) return;

    LOG_INFO("SceneRendererSubsystem: Shutting down scene rendering pipelines and buffers...");

    for (auto& buf : m_lightUBOBuffers) {
        buf.reset();
    }
    m_defaultWhiteTexture.reset();

    if (m_ctx && m_ctx->vulkanContext && m_ctx->vulkanContext->GetDevice() != VK_NULL_HANDLE) {
        VkDevice device = m_ctx->vulkanContext->GetDevice();

        for (VkPipeline* p : {&m_gridPipeline, &m_graphicsPipeline, &m_wireframePipeline}) {
            if (*p != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, *p, nullptr);
                *p = VK_NULL_HANDLE;
            }
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
        if (m_gridPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_gridPipelineLayout, nullptr);
            m_gridPipelineLayout = VK_NULL_HANDLE;
        }
        if (m_lightDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_lightDescriptorSetLayout, nullptr);
            m_lightDescriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_textureDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_textureDescriptorSetLayout, nullptr);
            m_textureDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    m_spirvCache.clear();
    m_ctx = nullptr;
    m_initialized = false;
    LOG_INFO("SceneRendererSubsystem: Teardown complete.");
}

const std::vector<uint32_t>& SceneRendererSubsystem::GetOrLoadSPIRV(const std::string& path) {
    auto it = m_spirvCache.find(path);
    if (it != m_spirvCache.end()) {
        return it->second;
    }

    std::vector<std::string> searchPaths = {
        path,
        "../" + path,
        "../../" + path,
        "build/" + path,
        "build/Debug/" + path
    };

    for (const auto& candidate : searchPaths) {
        std::ifstream file(candidate, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t fileSize = static_cast<size_t>(file.tellg());
            if (fileSize > 0 && fileSize % sizeof(uint32_t) == 0) {
                std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
                file.seekg(0);
                file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
                LOG_INFO("SceneRendererSubsystem: Successfully loaded shader SPIR-V from: " + candidate);
                m_spirvCache[path] = std::move(buffer);
                return m_spirvCache[path];
            }
        }
    }

    LOG_ERROR("SceneRendererSubsystem: Failed to open shader file across all candidate paths: " + path);
    throw std::runtime_error("Failed to open shader file: " + path);
}

void SceneRendererSubsystem::InitRenderResources() {
    if (!m_ctx || !m_ctx->vulkanContext || !m_ctx->descriptorAllocator) return;

    // 1. Light Descriptor Set Layout (Set 0)
    if (!m_lightDescriptorSetLayout) {
        VkDescriptorSetLayoutBinding lightBinding{};
        lightBinding.binding = 0;
        lightBinding.descriptorCount = 1;
        lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &lightBinding;

        const VkResult res = vkCreateDescriptorSetLayout(m_ctx->vulkanContext->GetDevice(), &layoutInfo, nullptr, &m_lightDescriptorSetLayout);
        CHECK_VK_RESULT(res, "Failed to create light descriptor set layout");
    }

    // 2. Texture Descriptor Set Layout (Set 1)
    if (!m_textureDescriptorSetLayout) {
        VkDescriptorSetLayoutBinding texBinding{};
        texBinding.binding = 0;
        texBinding.descriptorCount = 1;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &texBinding;

        const VkResult res = vkCreateDescriptorSetLayout(m_ctx->vulkanContext->GetDevice(), &layoutInfo, nullptr, &m_textureDescriptorSetLayout);
        CHECK_VK_RESULT(res, "Failed to create texture descriptor set layout");
    }

    // 3. Light UBO Buffers & Descriptor Sets
    for (uint32_t i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; ++i) {
        if (!m_lightUBOBuffers[i]) {
            m_lightUBOBuffers[i] = std::make_unique<Buffer>(
                *m_ctx->vulkanContext,
                sizeof(LightUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            );
        }
        if (m_lightDescriptorSets[i] == VK_NULL_HANDLE) {
            m_lightDescriptorSets[i] = m_ctx->descriptorAllocator->Allocate(m_lightDescriptorSetLayout);
            DescriptorWriter writer;
            writer.WriteBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_lightUBOBuffers[i]->GetBuffer(), sizeof(LightUBO));
            writer.UpdateSet(*m_ctx->vulkanContext, m_lightDescriptorSets[i]);
        }
    }

    // 4. Default White Texture (Set 1)
    if (!m_defaultWhiteTexture) {
        m_defaultWhiteTexture = Texture::CreateWhiteTexture(*m_ctx->vulkanContext, m_textureDescriptorSetLayout, *m_ctx->descriptorAllocator);
    }

    // 5. Mesh Pipeline Layout (Set 0: Light, Set 1: Texture)
    if (!m_pipelineLayout) {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(PushConstants);

        VkDescriptorSetLayout setLayouts[2] = { m_lightDescriptorSetLayout, m_textureDescriptorSetLayout };

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        const VkResult res = vkCreatePipelineLayout(m_ctx->vulkanContext->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout);
        CHECK_VK_RESULT(res, "Failed to create pipeline layout");
    }

    // 6. Grid Pipeline Layout
    if (!m_gridPipelineLayout) {
        VkPushConstantRange gridPushConstant{};
        gridPushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        gridPushConstant.offset = 0;
        gridPushConstant.size = sizeof(GridPushConstants);

        VkPipelineLayoutCreateInfo gridLayoutInfo{};
        gridLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        gridLayoutInfo.setLayoutCount = 0;
        gridLayoutInfo.pSetLayouts = nullptr;
        gridLayoutInfo.pushConstantRangeCount = 1;
        gridLayoutInfo.pPushConstantRanges = &gridPushConstant;

        const VkResult res = vkCreatePipelineLayout(m_ctx->vulkanContext->GetDevice(), &gridLayoutInfo, nullptr, &m_gridPipelineLayout);
        CHECK_VK_RESULT(res, "Failed to create grid pipeline layout");
    }
}

void SceneRendererSubsystem::CreateRenderPipeline(VkSampleCountFlagBits msaaSamples) {
    if (!m_ctx || !m_ctx->vulkanContext) return;

    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_ctx->vulkanContext->GetDevice(), m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }
    if (m_wireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_ctx->vulkanContext->GetDevice(), m_wireframePipeline, nullptr);
        m_wireframePipeline = VK_NULL_HANDLE;
    }

    const auto& vCode = GetOrLoadSPIRV("shaders/compiled/mesh.vert.spv");
    const auto& fCode = GetOrLoadSPIRV("shaders/compiled/mesh.frag.spv");

    VkShaderModule vertModule = PipelineBuilder::CreateShaderModule(*m_ctx->vulkanContext, vCode);
    VkShaderModule fragModule = PipelineBuilder::CreateShaderModule(*m_ctx->vulkanContext, fCode);

    const std::vector<VkVertexInputAttributeDescription> attribs = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = static_cast<uint32_t>(offsetof(Vertex, position)) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = static_cast<uint32_t>(offsetof(Vertex, normal)) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = static_cast<uint32_t>(offsetof(Vertex, uv)) }
    };

    m_currentPipelineMSAASamples = msaaSamples;

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule)
           .SetVertexInput(Vertex::GetBindingDescriptions(), attribs)
           .SetColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
           .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
           .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
           .SetMultisampling(msaaSamples, false)
           .EnableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    m_graphicsPipeline = builder.Build(*m_ctx->vulkanContext, m_pipelineLayout);

    builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
    m_wireframePipeline = builder.Build(*m_ctx->vulkanContext, m_pipelineLayout);

    vkDestroyShaderModule(m_ctx->vulkanContext->GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_ctx->vulkanContext->GetDevice(), fragModule, nullptr);
}

void SceneRendererSubsystem::CreateGridPipeline(VkSampleCountFlagBits msaaSamples) {
    if (!m_ctx || !m_ctx->vulkanContext) return;

    if (m_gridPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_ctx->vulkanContext->GetDevice(), m_gridPipeline, nullptr);
        m_gridPipeline = VK_NULL_HANDLE;
    }

    const auto& vCode = GetOrLoadSPIRV("shaders/compiled/grid.vert.spv");
    const auto& fCode = GetOrLoadSPIRV("shaders/compiled/grid.frag.spv");

    VkShaderModule vertModule = PipelineBuilder::CreateShaderModule(*m_ctx->vulkanContext, vCode);
    VkShaderModule fragModule = PipelineBuilder::CreateShaderModule(*m_ctx->vulkanContext, fCode);

    PipelineBuilder gridBuilder;
    gridBuilder.SetShaders(vertModule, fragModule)
               .SetColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
               .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
               .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
               .SetMultisampling(msaaSamples, false)
               .SetDepthTest(true, false, VK_COMPARE_OP_LESS_OR_EQUAL)
               .EnableAlphaBlending();

    m_gridPipeline = gridBuilder.Build(*m_ctx->vulkanContext, m_gridPipelineLayout);

    vkDestroyShaderModule(m_ctx->vulkanContext->GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_ctx->vulkanContext->GetDevice(), fragModule, nullptr);
}

void SceneRendererSubsystem::UpdateLightUBO(uint32_t currentFrame, SceneNode* rootNode, const glm::vec3& cameraPos) {
    uint32_t idx = currentFrame % Swapchain::MAX_FRAMES_IN_FLIGHT;
    if (!m_lightUBOBuffers[idx] || !rootNode) return;

    LightUBO ubo{};
    ubo.cameraPos = glm::vec4(cameraPos, 0.0f);

    std::vector<LightData> activeLights;

    std::function<void(SceneNode*, const glm::mat4&)> collectLights = [&](SceneNode* node, const glm::mat4& parentTransform) {
        if (!node || !node->visible) return;

        glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();

        if (node->lightComponent) {
            glm::vec3 worldPos = glm::vec3(worldTransform[3]);
            glm::mat3 rotMat = glm::mat3(worldTransform);
            glm::vec3 worldDir = rotMat * node->lightComponent->direction;

            if (activeLights.size() < MAX_LIGHTS) {
                activeLights.push_back(node->lightComponent->GetGPUData(worldPos, worldDir));
            }
        }

        for (const auto& child : node->GetChildren()) {
            collectLights(child.get(), worldTransform);
        }
    };

    collectLights(rootNode, glm::mat4(1.0f));

    ubo.cameraPos.w = static_cast<float>(activeLights.size());
    for (size_t i = 0; i < activeLights.size(); ++i) {
        ubo.lights[i] = activeLights[i];
    }

    m_lightUBOBuffers[idx]->CopyToBuffer(&ubo, sizeof(LightUBO));
}

void SceneRendererSubsystem::DrawSceneNode(VkCommandBuffer cmd, SceneNode* node, const glm::mat4& parentTransform,
                                          uint32_t currentFrame, const Camera& camera) {
    if (!node || !node->visible) return;

    glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();
    std::shared_ptr<MeshComponent> targetMesh = node->mesh;

    if (targetMesh) {
        VkDescriptorSet textureDS = targetMesh->HasTexture() ?
            targetMesh->GetTexture()->GetDescriptorSet() :
            m_defaultWhiteTexture->GetDescriptorSet();

        VkDescriptorSet sets[2] = {
            m_lightDescriptorSets[currentFrame % Swapchain::MAX_FRAMES_IN_FLIGHT],
            textureDS
        };

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                0, 2, sets, 0, nullptr);

        PushConstants push{};
        push.model = worldTransform;
        push.mvp   = camera.GetViewProjectionMatrix() * worldTransform;
        push.baseColorFactor = targetMesh->GetBaseColorFactor();
        push.emissiveFactor  = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        push.useTexture = targetMesh->HasTexture() ? 1 : 0;
        push.shininess = 32.0f;
        push.specularStrength = 0.5f;
        push.ambientStrength = 0.15f;

        // 1. Shaded Solid Pass
        if (node->wireframeMode != WireframeMode::WireframeOnly) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
            vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &push);
            targetMesh->Draw(cmd);
        }

        // 2. Wireframe Pass
        if (node->wireframeMode == WireframeMode::Overlay || node->wireframeMode == WireframeMode::WireframeOnly) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_wireframePipeline);
            PushConstants wirePush = push;
            if (node->wireframeMode == WireframeMode::Overlay) {
                wirePush.baseColorFactor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                wirePush.useTexture = 0;
            }
            vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &wirePush);
            targetMesh->Draw(cmd);
        }
    }

    for (const auto& child : node->GetChildren()) {
        DrawSceneNode(cmd, child.get(), worldTransform, currentFrame, camera);
    }
}

void SceneRendererSubsystem::RenderViewportOffscreen(VkCommandBuffer cmd, uint32_t currentFrame,
                                                     ViewportPanel* viewport, SceneNode* rootNode,
                                                     const Camera& camera) {
    if (!viewport || viewport->GetColorImageView(currentFrame) == VK_NULL_HANDLE) return;

    UpdateLightUBO(currentFrame, rootNode, camera.GetPosition());

    // Rebuild pipelines if MSAA sample count changed
    if (m_currentPipelineMSAASamples != viewport->GetMSAASamples()) {
        if (m_ctx && m_ctx->vulkanContext) m_ctx->vulkanContext->WaitIdle();
        CreateRenderPipeline(viewport->GetMSAASamples());
        CreateGridPipeline(viewport->GetMSAASamples());
    }

    viewport->TransitionToColorAttachment(cmd, currentFrame);

    VkSampleCountFlagBits msaaSamples = viewport->GetMSAASamples();

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    if (msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        colorAttachment.imageView = viewport->GetMSAAColorImageView(currentFrame);
        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.resolveImageView = viewport->GetColorImageView(currentFrame);
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
        colorAttachment.imageView = viewport->GetColorImageView(currentFrame);
    }
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = (msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.12f, 0.14f, 0.18f, 1.0f };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = (msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? viewport->GetMSAADepthImageView(currentFrame) : viewport->GetDepthImageView(currentFrame);
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {viewport->GetWidth(), viewport->GetHeight()} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport vkViewport{};
    vkViewport.x = 0.0f;
    vkViewport.y = 0.0f;
    vkViewport.width  = static_cast<float>(viewport->GetWidth());
    vkViewport.height = static_cast<float>(viewport->GetHeight());
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vkViewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { viewport->GetWidth(), viewport->GetHeight() };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 1. Draw Scene Graph Meshes
    if (rootNode) {
        DrawSceneNode(cmd, rootNode, glm::mat4(1.0f), currentFrame, camera);
    }

    // 2. Draw 3D Infinite Ground Grid (if enabled)
    if (viewport->IsGridVisible() && m_gridPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gridPipeline);

        GridPushConstants gridPush{};
        gridPush.viewProj = camera.GetViewProjectionMatrix();
        gridPush.cameraPos = glm::vec4(camera.GetPosition(), 1.0f);
        gridPush.gridParams = glm::vec4(
            viewport->GetGridCellSize(),
            viewport->GetGridMajorStep(),
            viewport->GetGridFadeDistance(),
            viewport->GetGridOpacity()
        );

        vkCmdPushConstants(cmd, m_gridPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(GridPushConstants), &gridPush);

        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    viewport->TransitionToShaderRead(cmd, currentFrame);
}

} // namespace khepri
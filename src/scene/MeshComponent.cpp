#include "MeshComponent.h"
#include "../core/Logger.h"
#include <glm/gtc/constants.hpp>

MeshComponent::MeshComponent(VulkanContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : m_context(context), m_vertices(vertices), m_indices(indices) {
    
    if (vertices.empty() || indices.empty()) {
        LOG_WARN("MeshComponent initialized with empty vertex/index data");
        return;
    }

    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();

    // Create Staging Buffer for Vertices
    Buffer stagingVertex(context, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingVertex.CopyToBuffer(vertices.data(), vertexSize);

    // Create Device Local Vertex Buffer
    m_vertexBuffer = std::make_unique<Buffer>(
        context, vertexSize, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    Buffer::CopyBuffer(context, stagingVertex.GetBuffer(), m_vertexBuffer->GetBuffer(), vertexSize);

    // Create Staging Buffer for Indices
    Buffer stagingIndex(context, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingIndex.CopyToBuffer(indices.data(), indexSize);

    // Create Device Local Index Buffer
    m_indexBuffer = std::make_unique<Buffer>(
        context, indexSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    Buffer::CopyBuffer(context, stagingIndex.GetBuffer(), m_indexBuffer->GetBuffer(), indexSize);
}

void MeshComponent::Draw(VkCommandBuffer cmd) const {
    if (!m_vertexBuffer || !m_indexBuffer || m_indices.empty()) return;
    VkBuffer vertexBuffers[] = { m_vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
}

std::shared_ptr<MeshComponent> MeshComponent::CreateCube(VulkanContext& context, float size) {
    float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        // Front (+Z)
        { {-h, -h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {0, 1} },
        { { h, -h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1} },
        { { h,  h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {1, 0} },
        { {-h,  h,  h}, {0, 0, 1}, {1, 0, 0, 1}, {0, 0} },
        // Back (-Z)
        { { h, -h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 1} },
        { {-h, -h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1} },
        { {-h,  h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 0} },
        { { h,  h, -h}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 0} },
        // Top (+Y)
        { {-h,  h,  h}, {0, 1, 0}, {1, 0, 0, 1}, {0, 1} },
        { { h,  h,  h}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1} },
        { { h,  h, -h}, {0, 1, 0}, {1, 0, 0, 1}, {1, 0} },
        { {-h,  h, -h}, {0, 1, 0}, {1, 0, 0, 1}, {0, 0} },
        // Bottom (-Y)
        { {-h, -h, -h}, {0, -1, 0}, {1, 0, 0, 1}, {0, 1} },
        { { h, -h, -h}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1} },
        { { h, -h,  h}, {0, -1, 0}, {1, 0, 0, 1}, {1, 0} },
        { {-h, -h,  h}, {0, -1, 0}, {1, 0, 0, 1}, {0, 0} },
        // Right (+X)
        { { h, -h,  h}, {1, 0, 0}, {0, 0, -1, 1}, {0, 1} },
        { { h, -h, -h}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1} },
        { { h,  h, -h}, {1, 0, 0}, {0, 0, -1, 1}, {1, 0} },
        { { h,  h,  h}, {1, 0, 0}, {0, 0, -1, 1}, {0, 0} },
        // Left (-X)
        { {-h, -h, -h}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 1} },
        { {-h, -h,  h}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1} },
        { {-h,  h,  h}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 0} },
        { {-h,  h, -h}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 0} }
    };

    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t offset = i * 4;
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 3);
        indices.push_back(offset + 0);
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

std::shared_ptr<MeshComponent> MeshComponent::CreateSphere(VulkanContext& context, float radius, uint32_t sectors, uint32_t stacks) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float sectorStep = 2.0f * glm::pi<float>() / sectors;
    float stackStep = glm::pi<float>() / stacks;

    for (uint32_t i = 0; i <= stacks; ++i) {
        float stackAngle = glm::pi<float>() / 2.0f - i * stackStep;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;

            Vertex v;
            v.position.x = xy * cosf(sectorAngle);
            v.position.y = z;
            v.position.z = xy * sinf(sectorAngle);

            v.normal = glm::normalize(v.position);
            v.uv.x = (float)j / sectors;
            v.uv.y = (float)i / stacks;

            vertices.push_back(v);
        }
    }

    for (uint32_t i = 0; i < stacks; ++i) {
        uint32_t k1 = i * (sectors + 1);
        uint32_t k2 = k1 + sectors + 1;

        for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

std::shared_ptr<MeshComponent> MeshComponent::CreatePlane(VulkanContext& context, float size, uint32_t subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float step = size / subdivisions;
    float half = size * 0.5f;

    for (uint32_t i = 0; i <= subdivisions; ++i) {
        float z = -half + i * step;
        for (uint32_t j = 0; j <= subdivisions; ++j) {
            float x = -half + j * step;
            Vertex v;
            v.position = glm::vec3(x, 0.0f, z);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2((float)j / subdivisions, (float)i / subdivisions);
            vertices.push_back(v);
        }
    }

    for (uint32_t i = 0; i < subdivisions; ++i) {
        for (uint32_t j = 0; j < subdivisions; ++j) {
            uint32_t row1 = i * (subdivisions + 1);
            uint32_t row2 = (i + 1) * (subdivisions + 1);

            indices.push_back(row1 + j);
            indices.push_back(row2 + j);
            indices.push_back(row1 + j + 1);

            indices.push_back(row1 + j + 1);
            indices.push_back(row2 + j);
            indices.push_back(row2 + j + 1);
        }
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

std::shared_ptr<MeshComponent> MeshComponent::CreateCylinder(VulkanContext& context, float radius, float height, uint32_t sectors) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float sectorStep = 2.0f * glm::pi<float>() / sectors;
    float h2 = height * 0.5f;

    // Side vertices
    for (uint32_t i = 0; i <= sectors; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        glm::vec3 norm = glm::normalize(glm::vec3(x, 0.0f, z));

        Vertex vTop;
        vTop.position = glm::vec3(x, h2, z);
        vTop.normal = norm;
        vTop.uv = glm::vec2((float)i / sectors, 0.0f);
        vertices.push_back(vTop);

        Vertex vBot;
        vBot.position = glm::vec3(x, -h2, z);
        vBot.normal = norm;
        vBot.uv = glm::vec2((float)i / sectors, 1.0f);
        vertices.push_back(vBot);
    }

    for (uint32_t i = 0; i < sectors; ++i) {
        uint32_t top1 = i * 2;
        uint32_t bot1 = top1 + 1;
        uint32_t top2 = (i + 1) * 2;
        uint32_t bot2 = top2 + 1;

        indices.push_back(top1);
        indices.push_back(bot1);
        indices.push_back(top2);

        indices.push_back(top2);
        indices.push_back(bot1);
        indices.push_back(bot2);
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

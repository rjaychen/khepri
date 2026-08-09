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

glm::vec3 MeshComponent::GetBoundingBoxCenter() const {
    if (m_vertices.empty()) return glm::vec3(0.0f);
    glm::vec3 minP = m_vertices[0].position;
    glm::vec3 maxP = m_vertices[0].position;
    for (const auto& v : m_vertices) {
        minP = glm::min(minP, v.position);
        maxP = glm::max(maxP, v.position);
    }
    return (minP + maxP) * 0.5f;
}

float MeshComponent::GetBoundingBoxRadius() const {
    if (m_vertices.empty()) return 1.0f;
    glm::vec3 minP = m_vertices[0].position;
    glm::vec3 maxP = m_vertices[0].position;
    for (const auto& v : m_vertices) {
        minP = glm::min(minP, v.position);
        maxP = glm::max(maxP, v.position);
    }
    return std::max(0.5f, glm::length(maxP - minP) * 0.5f);
}

void MeshComponent::Draw(VkCommandBuffer cmd) const {
    if (!m_vertexBuffer || !m_indexBuffer || m_indices.empty()) return;

    VkBuffer vBufs[] = { m_vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vBufs, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
}

std::shared_ptr<MeshComponent> MeshComponent::CreateCube(VulkanContext& context, float size) {
    float h = size * 0.5f;
    auto makeV = [](glm::vec3 p, glm::vec3 n, glm::vec2 uv) {
        Vertex v{};
        v.position = p;
        v.normal = n;
        v.uv = uv;
        return v;
    };

    std::vector<Vertex> vertices = {
        // Front face (+Z)
        makeV({-h, -h,  h}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}),
        makeV({ h, -h,  h}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}),
        makeV({ h,  h,  h}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}),
        makeV({-h,  h,  h}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}),
        // Back face (-Z)
        makeV({ h, -h, -h}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}),
        makeV({-h, -h, -h}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}),
        makeV({-h,  h, -h}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}),
        makeV({ h,  h, -h}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}),
        // Top face (+Y)
        makeV({-h,  h,  h}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}),
        makeV({ h,  h,  h}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}),
        makeV({ h,  h, -h}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}),
        makeV({-h,  h, -h}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}),
        // Bottom face (-Y)
        makeV({-h, -h, -h}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}),
        makeV({ h, -h, -h}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}),
        makeV({ h, -h,  h}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}),
        makeV({-h, -h,  h}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}),
        // Right face (+X)
        makeV({ h, -h,  h}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}),
        makeV({ h, -h, -h}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}),
        makeV({ h,  h, -h}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}),
        makeV({ h,  h,  h}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}),
        // Left face (-X)
        makeV({-h, -h, -h}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}),
        makeV({-h, -h,  h}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}),
        makeV({-h,  h,  h}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}),
        makeV({-h,  h, -h}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f})
    };

    std::vector<uint32_t> indices = {
         0, 1, 2,  2, 3, 0,
         4, 5, 6,  6, 7, 4,
         8, 9,10, 10,11, 8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };

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

    float halfSize = size * 0.5f;
    float step = size / subdivisions;

    for (uint32_t z = 0; z <= subdivisions; ++z) {
        for (uint32_t x = 0; x <= subdivisions; ++x) {
            Vertex v;
            v.position = glm::vec3(-halfSize + x * step, 0.0f, -halfSize + z * step);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2((float)x / subdivisions, (float)z / subdivisions);
            vertices.push_back(v);
        }
    }

    for (uint32_t z = 0; z < subdivisions; ++z) {
        for (uint32_t x = 0; x < subdivisions; ++x) {
            uint32_t row1 = z * (subdivisions + 1);
            uint32_t row2 = (z + 1) * (subdivisions + 1);

            indices.push_back(row1 + x);
            indices.push_back(row2 + x);
            indices.push_back(row1 + x + 1);

            indices.push_back(row1 + x + 1);
            indices.push_back(row2 + x);
            indices.push_back(row2 + x + 1);
        }
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

std::shared_ptr<MeshComponent> MeshComponent::CreateCylinder(VulkanContext& context, float radius, float height, uint32_t sectors) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float sectorStep = 2.0f * glm::pi<float>() / sectors;
    float h2 = height * 0.5f;

    // 1. Side Quad Strip
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

    // 2. Top Cap Center & Fan Vertices
    uint32_t topCenterIndex = static_cast<uint32_t>(vertices.size());
    Vertex topCenter;
    topCenter.position = glm::vec3(0.0f, h2, 0.0f);
    topCenter.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    topCenter.uv = glm::vec2(0.5f, 0.5f);
    vertices.push_back(topCenter);

    uint32_t topRingStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i <= sectors; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        Vertex v;
        v.position = glm::vec3(x, h2, z);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = glm::vec2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
        vertices.push_back(v);
    }

    for (uint32_t i = 0; i < sectors; ++i) {
        indices.push_back(topCenterIndex);
        indices.push_back(topRingStart + i + 1);
        indices.push_back(topRingStart + i);
    }

    // 3. Bottom Cap Center & Fan Vertices
    uint32_t botCenterIndex = static_cast<uint32_t>(vertices.size());
    Vertex botCenter;
    botCenter.position = glm::vec3(0.0f, -h2, 0.0f);
    botCenter.normal = glm::vec3(0.0f, -1.0f, 0.0f);
    botCenter.uv = glm::vec2(0.5f, 0.5f);
    vertices.push_back(botCenter);

    uint32_t botRingStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i <= sectors; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        Vertex v;
        v.position = glm::vec3(x, -h2, z);
        v.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        v.uv = glm::vec2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
        vertices.push_back(v);
    }

    for (uint32_t i = 0; i < sectors; ++i) {
        indices.push_back(botCenterIndex);
        indices.push_back(botRingStart + i);
        indices.push_back(botRingStart + i + 1);
    }

    return std::make_shared<MeshComponent>(context, vertices, indices);
}

#pragma once

#include <volk.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "../vulkan/Buffer.h"
#include "../vulkan/VulkanContext.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec2 uv;
    glm::uvec4 jointIndices{0, 0, 0, 0};
    glm::vec4 jointWeights{1.0f, 0.0f, 0.0f, 0.0f};

    static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(Vertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindings;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(6);
        // Position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, position);
        // Normal
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, normal);
        // Tangent
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, tangent);
        // UV
        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[3].offset = offsetof(Vertex, uv);
        // Joint Indices
        attributes[4].binding = 0;
        attributes[4].location = 4;
        attributes[4].format = VK_FORMAT_R32G32B32A32_UINT;
        attributes[4].offset = offsetof(Vertex, jointIndices);
        // Joint Weights
        attributes[5].binding = 0;
        attributes[5].location = 5;
        attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[5].offset = offsetof(Vertex, jointWeights);

        return attributes;
    }
};

class MeshComponent {
public:
    MeshComponent(VulkanContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~MeshComponent() = default;

    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_indices.size()); }

    VkBuffer GetVertexBuffer() const { return m_vertexBuffer->GetBuffer(); }
    VkBuffer GetIndexBuffer() const { return m_indexBuffer->GetBuffer(); }

    void Draw(VkCommandBuffer cmd) const;

    // Standard Primitive Factory Generators
    static std::shared_ptr<MeshComponent> CreateCube(VulkanContext& context, float size = 1.0f);
    static std::shared_ptr<MeshComponent> CreateSphere(VulkanContext& context, float radius = 0.5f, uint32_t sectors = 32, uint32_t stacks = 16);
    static std::shared_ptr<MeshComponent> CreatePlane(VulkanContext& context, float size = 10.0f, uint32_t gridSubdivisions = 10);
    static std::shared_ptr<MeshComponent> CreateCylinder(VulkanContext& context, float radius = 0.5f, float height = 1.0f, uint32_t sectors = 32);

private:
    VulkanContext& m_context;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;
};

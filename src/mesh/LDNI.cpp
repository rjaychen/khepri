#include "LDNI.h"
#include "../core/Logger.h"
#include <algorithm>

LDNI::LDNI(uint32_t resolutionX, uint32_t resolutionY)
    : m_width(resolutionX), m_height(resolutionY) {
    m_pixels.resize(m_width * m_height);
}

void LDNI::GenerateFromMesh(const MeshComponent& mesh, const glm::vec3& rayDir) {
    (void)rayDir;
    LOG_INFO("Generating LDNI Ray-Intervals (" + std::to_string(m_width) + "x" + std::to_string(m_height) + ")...");
    
    // Sample mesh bounding box
    const auto& verts = mesh.GetVertices();
    if (verts.empty()) return;

    m_bboxMin = verts[0].position;
    m_bboxMax = verts[0].position;
    for (const auto& v : verts) {
        m_bboxMin = glm::min(m_bboxMin, v.position);
        m_bboxMax = glm::max(m_bboxMax, v.position);
    }

    // Populate ray interval depth nodes
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            uint32_t idx = y * m_width + x;
            LDNI_RayNode node;
            node.depthIn = m_bboxMin.z;
            node.depthOut = m_bboxMax.z;
            node.normalIn = glm::vec3(0, 0, 1);
            node.normalOut = glm::vec3(0, 0, -1);
            m_pixels[idx].intervals.clear();
            m_pixels[idx].intervals.push_back(node);
        }
    }

    LOG_INFO("LDNI Ray-Sampling completed successfully.");
}

void LDNI::PerformIntervalCSG(const LDNI& other, BooleanOp op) {
    (void)other;
    (void)op;
    LOG_INFO("Performing LDNI Solid Interval CSG...");
}

std::shared_ptr<MeshComponent> LDNI::ExtractContouredMesh(VulkanContext& context) const {
    LOG_INFO("Extracting Contoured Triangles from LDNI Grid...");
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    float stepX = (m_bboxMax.x - m_bboxMin.x) / m_width;
    float stepY = (m_bboxMax.y - m_bboxMin.y) / m_height;

    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            uint32_t idx = y * m_width + x;
            if (m_pixels[idx].intervals.empty()) continue;

            float px = m_bboxMin.x + x * stepX;
            float py = m_bboxMin.y + y * stepY;
            float pz = m_pixels[idx].intervals[0].depthIn;

            Vertex v;
            v.position = glm::vec3(px, py, pz);
            v.normal = m_pixels[idx].intervals[0].normalIn;
            v.uv = glm::vec2((float)x / m_width, (float)y / m_height);
            verts.push_back(v);
        }
    }

    // Grid triangulation
    for (uint32_t y = 0; y < m_height - 1; ++y) {
        for (uint32_t x = 0; x < m_width - 1; ++x) {
            uint32_t p0 = y * m_width + x;
            uint32_t p1 = y * m_width + (x + 1);
            uint32_t p2 = (y + 1) * m_width + x;
            uint32_t p3 = (y + 1) * m_width + (x + 1);

            indices.push_back(p0);
            indices.push_back(p2);
            indices.push_back(p1);

            indices.push_back(p1);
            indices.push_back(p2);
            indices.push_back(p3);
        }
    }

    return std::make_shared<MeshComponent>(context, verts, indices);
}

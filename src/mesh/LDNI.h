#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "../scene/MeshComponent.h"
#include "../vulkan/VulkanContext.h"
#include "MeshBoolean.h"

struct LDNI_RayNode {
    float depthIn;
    float depthOut;
    glm::vec3 normalIn;
    glm::vec3 normalOut;
};

struct LDNI_Pixel {
    std::vector<LDNI_RayNode> intervals;
};

class LDNI {
public:
    LDNI(uint32_t resolutionX = 128, uint32_t resolutionY = 128);

    void GenerateFromMesh(const MeshComponent& mesh, const glm::vec3& rayDirection = glm::vec3(0, 0, -1));
    
    void PerformIntervalCSG(const LDNI& other, BooleanOp op);

    std::shared_ptr<MeshComponent> ExtractContouredMesh(VulkanContext& context) const;

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    const std::vector<LDNI_Pixel>& GetPixels() const { return m_pixels; }

private:
    uint32_t m_width;
    uint32_t m_height;
    glm::vec3 m_bboxMin{-1.0f};
    glm::vec3 m_bboxMax{1.0f};
    std::vector<LDNI_Pixel> m_pixels;
};

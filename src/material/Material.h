#pragma once

#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include "../vulkan/Texture.h"

namespace khepri {

class Material {
public:
    Material(const std::string& name = "DefaultMaterial");
    ~Material() = default;

    const std::string& GetName() const { return m_name; }

    void SetBaseColorFactor(const glm::vec4& color) { m_baseColorFactor = color; }
    const glm::vec4& GetBaseColorFactor() const { return m_baseColorFactor; }

    void SetEmissiveFactor(const glm::vec4& emissive) { m_emissiveFactor = emissive; }
    const glm::vec4& GetEmissiveFactor() const { return m_emissiveFactor; }

    void SetRoughness(float roughness) { m_roughness = roughness; }
    float GetRoughness() const { return m_roughness; }

    void SetMetallic(float metallic) { m_metallic = metallic; }
    float GetMetallic() const { return m_metallic; }

    void SetAlbedoTexture(std::shared_ptr<Texture> texture) { m_albedoTexture = texture; }
    std::shared_ptr<Texture> GetAlbedoTexture() const { return m_albedoTexture; }

private:
    std::string m_name;
    glm::vec4 m_baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 m_emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    std::shared_ptr<Texture> m_albedoTexture = nullptr;
};

} // namespace khepri

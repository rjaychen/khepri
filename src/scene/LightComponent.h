#pragma once

#include "Light.h"
#include <glm/glm.hpp>
#include <cmath>

class LightComponent {
public:
    explicit LightComponent(LightType lightType = LightType::Point)
        : type(lightType) {}
    virtual ~LightComponent() = default;

    LightType type = LightType::Point;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    // Attenuation coefficients (for Point & Spot)
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.09f;
    float quadraticAttenuation = 0.032f;

    // Spot Light Cone Angles (in degrees)
    float innerCutoffAngle = 15.0f;
    float outerCutoffAngle = 25.0f;

    // Effective influence range / helper distance (in meters)
    float range = 10.0f;

    [[nodiscard]] float GetRange() const noexcept { return range; }
    void SetRange(float r) noexcept { range = (r > 0.01f) ? r : 0.01f; }

    // Local direction vector (for Directional & Spot lights)
    glm::vec3 direction{0.0f, -1.0f, 0.0f};

    virtual LightData GetGPUData(const glm::vec3& worldPos, const glm::vec3& worldDir) const {
        LightData data;
        data.position = glm::vec4(worldPos, static_cast<float>(type));
        glm::vec3 normDir = (glm::length(worldDir) > 1e-4f) ? glm::normalize(worldDir) : glm::vec3(0, -1, 0);
        data.direction = glm::vec4(normDir, std::cos(glm::radians(innerCutoffAngle)));
        data.color = glm::vec4(color, intensity);
        data.params = glm::vec4(constantAttenuation, linearAttenuation, quadraticAttenuation, std::cos(glm::radians(outerCutoffAngle)));
        return data;
    }
};

class DirectionalLightComponent : public LightComponent {
public:
    explicit DirectionalLightComponent(const glm::vec3& lightColor = glm::vec3(1.0f, 0.95f, 0.85f), float lightIntensity = 1.0f)
        : LightComponent(LightType::Directional) {
        color = lightColor;
        intensity = lightIntensity;
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
    }
    virtual ~DirectionalLightComponent() override = default;
};

class PointLightComponent : public LightComponent {
public:
    explicit PointLightComponent(const glm::vec3& lightColor = glm::vec3(1.0f, 0.8f, 0.4f), float lightIntensity = 2.0f, float lightRange = 10.0f)
        : LightComponent(LightType::Point) {
        color = lightColor;
        intensity = lightIntensity;
        range = lightRange;
    }
    virtual ~PointLightComponent() override = default;
};

class SpotLightComponent : public PointLightComponent {
public:
    explicit SpotLightComponent(const glm::vec3& lightColor = glm::vec3(0.4f, 0.8f, 1.0f), float lightIntensity = 3.0f, float lightRange = 10.0f,
                                float innerAngle = 15.0f, float outerAngle = 25.0f)
        : PointLightComponent(lightColor, lightIntensity, lightRange) {
        type = LightType::Spot;
        innerCutoffAngle = innerAngle;
        outerCutoffAngle = outerAngle;
        direction = glm::vec3(0.0f, -1.0f, 0.0f);
    }
    virtual ~SpotLightComponent() override = default;
};

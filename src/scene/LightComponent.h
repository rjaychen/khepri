#pragma once

#include "Light.h"
#include <glm/glm.hpp>
#include <cmath>

class LightComponent {
public:
    LightComponent(LightType lightType = LightType::Point)
        : type(lightType) {}

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

    // Local direction vector (for Directional & Spot lights)
    glm::vec3 direction{0.0f, -1.0f, 0.0f};

    LightData GetGPUData(const glm::vec3& worldPos, const glm::vec3& worldDir) const {
        LightData data;
        data.position = glm::vec4(worldPos, static_cast<float>(type));
        glm::vec3 normDir = (glm::length(worldDir) > 1e-4f) ? glm::normalize(worldDir) : glm::vec3(0, -1, 0);
        data.direction = glm::vec4(normDir, std::cos(glm::radians(innerCutoffAngle)));
        data.color = glm::vec4(color, intensity);
        data.params = glm::vec4(constantAttenuation, linearAttenuation, quadraticAttenuation, std::cos(glm::radians(outerCutoffAngle)));
        return data;
    }
};

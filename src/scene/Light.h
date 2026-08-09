#pragma once

#include <glm/glm.hpp>
#include <cstdint>

enum class LightType : int32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

// Align to std140 layout (16-byte boundaries)
struct LightData {
    glm::vec4 position{0.0f, 10.0f, 0.0f, 0.0f};  // xyz = pos, w = (float)LightType
    glm::vec4 direction{0.0f, -1.0f, 0.0f, 0.9659f}; // xyz = dir, w = cos(innerAngle)
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};      // rgb = color, w = intensity
    glm::vec4 params{1.0f, 0.09f, 0.032f, 0.9063f}; // x = constant, y = linear, z = quadratic, w = cos(outerAngle)
};

constexpr uint32_t MAX_LIGHTS = 16;

struct LightUBO {
    glm::vec4 cameraPos{0.0f, 0.0f, 5.0f, 1.0f}; // xyz = camera position, w = numLights
    LightData lights[MAX_LIGHTS];
};

struct MaterialParams {
    float shininess = 32.0f;
    float specularStrength = 0.5f;
    float ambientStrength = 0.15f;
};

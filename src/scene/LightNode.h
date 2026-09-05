#pragma once

#include "SceneNode.h"
#include "LightComponent.h"
#include <memory>
#include <string>
#include <glm/glm.hpp>

class LightNode : public SceneNode {
public:
    explicit LightNode(const std::string& nodeName, LightType lightType);
    virtual ~LightNode() override = default;

    [[nodiscard]] bool IsLightNode() const noexcept override { return true; }
    [[nodiscard]] LightType GetLightType() const noexcept {
        return lightComponent ? lightComponent->type : LightType::Point;
    }

    [[nodiscard]] glm::vec3 GetLightColor() const noexcept {
        return lightComponent ? lightComponent->color : glm::vec3(1.0f);
    }
    void SetLightColor(const glm::vec3& c) noexcept {
        if (lightComponent) lightComponent->color = c;
    }

    [[nodiscard]] float GetLightIntensity() const noexcept {
        return lightComponent ? lightComponent->intensity : 1.0f;
    }
    void SetLightIntensity(float i) noexcept {
        if (lightComponent) lightComponent->intensity = i;
    }
};

class DirectionalLightNode : public LightNode {
public:
    explicit DirectionalLightNode(
        const std::string& nodeName = "Directional Light",
        const glm::vec3& pos = glm::vec3(0.0f, 10.0f, 0.0f),
        const glm::vec3& rotDeg = glm::vec3(-45.0f, 45.0f, 0.0f),
        const glm::vec3& col = glm::vec3(1.0f, 0.95f, 0.85f),
        float intens = 1.0f
    );
    virtual ~DirectionalLightNode() override = default;

    [[nodiscard]] std::shared_ptr<DirectionalLightComponent> GetDirectionalLightComponent() const noexcept {
        return std::dynamic_pointer_cast<DirectionalLightComponent>(lightComponent);
    }
};

class PointLightNode : public LightNode {
public:
    explicit PointLightNode(
        const std::string& nodeName = "Point Light",
        const glm::vec3& pos = glm::vec3(0.0f, 3.0f, 0.0f),
        const glm::vec3& col = glm::vec3(1.0f, 0.8f, 0.4f),
        float intens = 2.0f,
        float lightRange = 10.0f
    );
    virtual ~PointLightNode() override = default;

    [[nodiscard]] float GetRange() const noexcept {
        return lightComponent ? lightComponent->range : 10.0f;
    }
    void SetRange(float r) noexcept {
        if (lightComponent) lightComponent->SetRange(r);
    }

    [[nodiscard]] std::shared_ptr<PointLightComponent> GetPointLightComponent() const noexcept {
        return std::dynamic_pointer_cast<PointLightComponent>(lightComponent);
    }
};

class SpotLightNode : public LightNode {
public:
    explicit SpotLightNode(
        const std::string& nodeName = "Spot Light",
        const glm::vec3& pos = glm::vec3(0.0f, 4.0f, 2.0f),
        const glm::vec3& rotDeg = glm::vec3(-30.0f, 0.0f, 0.0f),
        const glm::vec3& col = glm::vec3(0.4f, 0.8f, 1.0f),
        float intens = 3.0f,
        float lightRange = 10.0f,
        float innerAngle = 15.0f,
        float outerAngle = 25.0f
    );
    virtual ~SpotLightNode() override = default;

    [[nodiscard]] float GetInnerCutoffAngle() const noexcept {
        return lightComponent ? lightComponent->innerCutoffAngle : 15.0f;
    }
    void SetInnerCutoffAngle(float a) noexcept {
        if (lightComponent) lightComponent->innerCutoffAngle = a;
    }

    [[nodiscard]] float GetOuterCutoffAngle() const noexcept {
        return lightComponent ? lightComponent->outerCutoffAngle : 25.0f;
    }
    void SetOuterCutoffAngle(float a) noexcept {
        if (lightComponent) lightComponent->outerCutoffAngle = a;
    }

    [[nodiscard]] std::shared_ptr<SpotLightComponent> GetSpotLightComponent() const noexcept {
        return std::dynamic_pointer_cast<SpotLightComponent>(lightComponent);
    }
};

#include "LightNode.h"

LightNode::LightNode(const std::string& nodeName, LightType lightType)
    : SceneNode(nodeName) {
    (void)lightType;
    // Light nodes do not own static mesh geometry (mesh remains nullptr).
    // The viewport renderer automatically draws the shared light gizmo mesh.
    mesh = nullptr;
}

DirectionalLightNode::DirectionalLightNode(
    const std::string& nodeName,
    const glm::vec3& pos,
    const glm::vec3& rotDeg,
    const glm::vec3& col,
    float intens
) : LightNode(nodeName, LightType::Directional) {
    position = pos;
    rotationDegrees = rotDeg;
    lightComponent = std::make_shared<DirectionalLightComponent>(col, intens);
    SyncPropertiesToTransform();
}

PointLightNode::PointLightNode(
    const std::string& nodeName,
    const glm::vec3& pos,
    const glm::vec3& col,
    float intens,
    float lightRange
) : LightNode(nodeName, LightType::Point) {
    position = pos;
    lightComponent = std::make_shared<PointLightComponent>(col, intens, lightRange);
    SyncPropertiesToTransform();
}

SpotLightNode::SpotLightNode(
    const std::string& nodeName,
    const glm::vec3& pos,
    const glm::vec3& rotDeg,
    const glm::vec3& col,
    float intens,
    float lightRange,
    float innerAngle,
    float outerAngle
) : LightNode(nodeName, LightType::Spot) {
    position = pos;
    rotationDegrees = rotDeg;
    lightComponent = std::make_shared<SpotLightComponent>(col, intens, lightRange, innerAngle, outerAngle);
    SyncPropertiesToTransform();
}

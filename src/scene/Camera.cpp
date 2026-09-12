#include "Camera.h"
#include "SceneNode.h"
#include <algorithm>

Camera::Camera(glm::vec3 eye, glm::vec3 target)
    : m_position(eye), m_target(target) {
    glm::vec3 forward = glm::normalize(target - eye);
    m_distance = std::max(0.1f, glm::length(eye - target));
    m_pitch = glm::degrees(asin(forward.y));
    m_yaw = glm::degrees(atan2(forward.z, forward.x));
}

void Camera::SetPerspective(float fovDegrees, float aspect, float zNear, float zFar) {
    m_fov = fovDegrees;
    m_aspect = aspect;
    m_near = zNear;
    m_far = zFar;
}

void Camera::SetViewportSize(float width, float height) {
    if (height > 0.0f) {
        m_aspect = width / height;
    }
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::GetProjectionMatrix() const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    proj[1][1] *= -1.0f; // Flip Y coordinate for Vulkan clip space standard!
    return proj;
}

glm::mat4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

void Camera::SetOrientation(float yawDegrees, float pitchDegrees) {
    m_yaw = yawDegrees;
    m_pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);
    UpdateVectors();
}

void Camera::Orbit(float deltaX, float deltaY) {
    float sensitivity = 0.25f;
    m_yaw += deltaX * sensitivity;
    m_pitch -= deltaY * sensitivity;

    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    UpdateVectors();
}

void Camera::Pan(float deltaX, float deltaY) {
    float sensitivity = m_distance * 0.002f;
    glm::vec3 forward = glm::normalize(m_target - m_position);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_up));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    glm::vec3 translation = (-right * deltaX + up * deltaY) * sensitivity;
    m_position += translation;
    m_target += translation;
}

void Camera::Zoom(float deltaZoom) {
    float sensitivity = 0.1f * m_distance;
    m_distance -= deltaZoom * sensitivity;
    if (m_distance < 0.1f) m_distance = 0.1f;
    UpdateVectors();
}

void Camera::Look(float deltaX, float deltaY) {
    float sensitivity = 0.15f;
    m_yaw += deltaX * sensitivity;
    m_pitch -= deltaY * sensitivity;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    float radYaw = glm::radians(m_yaw);
    float radPitch = glm::radians(m_pitch);

    glm::vec3 viewDir;
    viewDir.x = cos(radYaw) * cos(radPitch);
    viewDir.y = sin(radPitch);
    viewDir.z = sin(radYaw) * cos(radPitch);
    viewDir = glm::normalize(viewDir);

    m_target = m_position + viewDir * m_distance;
}

void Camera::Fly(glm::vec3 moveDir, float deltaTime) {
    if (glm::length(moveDir) < 0.001f) return;

    float clampedDelta = std::clamp(deltaTime, 0.0001f, 0.1f);
    glm::vec3 forward = glm::normalize(m_target - m_position);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_up));
    glm::vec3 up = m_up;

    glm::vec3 velocity = (forward * moveDir.z + right * moveDir.x + up * moveDir.y) * m_flySpeed * clampedDelta;
    m_position += velocity;
    m_target += velocity;
}

void Camera::AdjustFlySpeed(float deltaSpeed) {
    m_flySpeed += deltaSpeed * 0.5f;
    if (m_flySpeed < 0.2f) m_flySpeed = 0.2f;
    if (m_flySpeed > 100.0f) m_flySpeed = 100.0f;
}

void Camera::FocusOnTarget(glm::vec3 target, float distance) {
    m_target = target;
    m_distance = std::max(0.5f, distance);
    UpdateVectors();
}

void Camera::FocusOnNode(const SceneNode* node) {
    if (!node) {
        FocusOnTarget(glm::vec3(0.0f), 4.0f);
        return;
    }

    glm::mat4 worldMat = node->GetWorldTransform();
    glm::vec3 worldPos = glm::vec3(worldMat[3]);
    float distance = 3.5f;

    if (node->mesh) {
        glm::vec3 localCenter = node->mesh->GetBoundingBoxCenter();
        worldPos = glm::vec3(worldMat * glm::vec4(localCenter, 1.0f));
        distance = std::max(1.5f, node->mesh->GetBoundingBoxRadius() * 2.5f);
    }

    FocusOnTarget(worldPos, distance);
}

void Camera::UpdateVectors() {
    float radYaw = glm::radians(m_yaw);
    float radPitch = glm::radians(m_pitch);

    glm::vec3 viewDir;
    viewDir.x = cos(radYaw) * cos(radPitch);
    viewDir.y = sin(radPitch);
    viewDir.z = sin(radYaw) * cos(radPitch);

    m_position = m_target - glm::normalize(viewDir) * m_distance;
}

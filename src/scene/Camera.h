#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 eye = glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f));

    void SetPerspective(float fovDegrees, float aspect, float zNear, float zFar);
    void SetViewportSize(float width, float height);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;

    // Camera Navigation Controls
    void Orbit(float deltaX, float deltaY);
    void Pan(float deltaX, float deltaY);
    void Zoom(float deltaZoom);

    // Unreal Engine Flycam Controls
    void Look(float deltaX, float deltaY);
    void Fly(glm::vec3 moveDir, float deltaTime);
    void AdjustFlySpeed(float deltaSpeed);

    // MeshLab Focus Control & Scene Node Focus
    void FocusOnTarget(glm::vec3 target, float distance = 5.0f);
    void FocusOnNode(const class SceneNode* node);

    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetTarget() const { return m_target; }
    float GetFlySpeed() const { return m_flySpeed; }
    void SetFlySpeed(float speed) { m_flySpeed = std::max(0.1f, speed); }
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetDistance() const { return m_distance; }

private:
    void UpdateVectors();

    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    float m_fov = 45.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_distance = 5.0f;
    float m_flySpeed = 5.0f;
};

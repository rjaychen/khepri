#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>

class Camera;
class SceneNode;

namespace khepri {

enum class GizmoOperation {
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoMode {
    World,
    Local
};

enum class GizmoAxis {
    None = 0,
    X    = 1 << 0,
    Y    = 1 << 1,
    Z    = 1 << 2,
    XY   = (1 << 0) | (1 << 1),
    YZ   = (1 << 1) | (1 << 2),
    XZ   = (1 << 0) | (1 << 2),
    XYZ  = (1 << 0) | (1 << 1) | (1 << 2)
};

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
};

struct RayPlaneHit {
    bool hit = false;
    float distance = 0.0f;
    glm::vec3 point{0.0f};
};

struct SkewRayHit {
    bool valid = false;
    float rayParam = 0.0f;
    float axisParam = 0.0f;
    glm::vec3 rayPoint{0.0f};
    glm::vec3 axisPoint{0.0f};
    float distance = 0.0f;
};

class TransformGizmo {
public:
    TransformGizmo() = default;
    ~TransformGizmo() = default;

    // -------------------------------------------------------------
    // Pure Mathematical Functions (Unit-Testable & Robust)
    // -------------------------------------------------------------
    [[nodiscard]] static Ray ScreenToRay(
        const glm::vec2& mousePos,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        const glm::mat4& viewProjInv
    ) noexcept;

    [[nodiscard]] static glm::vec2 WorldToScreen(
        const glm::vec3& worldPos,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        const glm::mat4& viewProj,
        bool& outInFront
    ) noexcept;

    [[nodiscard]] static RayPlaneHit RayPlaneIntersection(
        const Ray& ray,
        const glm::vec3& planePoint,
        const glm::vec3& planeNormal
    ) noexcept;

    [[nodiscard]] static SkewRayHit RayAxisClosestPoint(
        const Ray& cursorRay,
        const glm::vec3& axisOrigin,
        const glm::vec3& axisDir
    ) noexcept;

    [[nodiscard]] static float SnapValue(float value, float snapStep) noexcept;
    [[nodiscard]] static glm::vec3 SnapVector(const glm::vec3& vec, float snapStep) noexcept;

    [[nodiscard]] static float CalculateRotationAngle(
        const glm::vec3& startHit,
        const glm::vec3& currentHit,
        const glm::vec3& center,
        const glm::vec3& axisNormal
    ) noexcept;

    // -------------------------------------------------------------
    // Gizmo Configuration & State
    // -------------------------------------------------------------
    [[nodiscard]] GizmoOperation GetOperation() const noexcept { return m_operation; }
    void SetOperation(GizmoOperation op) noexcept { m_operation = op; }

    [[nodiscard]] GizmoMode GetMode() const noexcept { return m_mode; }
    void SetMode(GizmoMode mode) noexcept { m_mode = mode; }

    [[nodiscard]] GizmoAxis GetHoveredAxis() const noexcept { return m_hoveredAxis; }
    [[nodiscard]] GizmoAxis GetActiveAxis() const noexcept { return m_activeAxis; }
    [[nodiscard]] bool IsUsing() const noexcept { return m_isDragging; }
    [[nodiscard]] bool IsHovered() const noexcept { return m_hoveredAxis != GizmoAxis::None; }

    // Snapping configuration
    bool snapEnabled = false;
    float translationSnap = 1.0f;  // 1.0 meter/unit default
    float rotationSnap = 15.0f;    // 15 degrees default
    float scaleSnap = 0.1f;        // 0.1x scale default

    // Visual styling constants
    float gizmoSize = 90.0f;       // Screen-space radius / size in pixels
    float handleRadius = 8.0f;     // Screen-space hit radius for handles

    // -------------------------------------------------------------
    // Main Update & Rendering Entry Point
    // -------------------------------------------------------------
    bool UpdateAndRender(
        ImDrawList* drawList,
        const Camera& camera,
        SceneNode* targetNode,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize
    );

    void ResetInteraction() noexcept {
        m_isDragging = false;
        m_activeAxis = GizmoAxis::None;
        m_hoveredAxis = GizmoAxis::None;
    }

private:
    void RenderTranslationGizmo(
        ImDrawList* drawList,
        const glm::vec2& centerScreen,
        const glm::vec3& centerWorld,
        const glm::vec3& axisX,
        const glm::vec3& axisY,
        const glm::vec3& axisZ,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize
    );

    void RenderRotationGizmo(
        ImDrawList* drawList,
        const glm::vec2& centerScreen,
        const glm::vec3& centerWorld,
        const glm::vec3& axisX,
        const glm::vec3& axisY,
        const glm::vec3& axisZ,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize
    );

    void RenderScaleGizmo(
        ImDrawList* drawList,
        const glm::vec2& centerScreen,
        const glm::vec3& centerWorld,
        const glm::vec3& axisX,
        const glm::vec3& axisY,
        const glm::vec3& axisZ,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize
    );

    GizmoOperation m_operation = GizmoOperation::Translate;
    GizmoMode m_mode = GizmoMode::World;
    GizmoAxis m_hoveredAxis = GizmoAxis::None;
    GizmoAxis m_activeAxis = GizmoAxis::None;

    bool m_isDragging = false;
    glm::vec2 m_dragStartMousePos{0.0f};
    glm::vec3 m_dragStartHitPoint{0.0f};
    float m_dragStartAxisParam = 0.0f;

    // Node initial state at the start of a drag operation
    glm::vec3 m_initialNodePosition{0.0f};
    glm::vec3 m_initialNodeRotation{0.0f};
    glm::vec3 m_initialNodeScale{1.0f};
};

} // namespace khepri

#include "TransformGizmo.h"
#include "../scene/Camera.h"
#include "../scene/SceneNode.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace khepri {

Ray TransformGizmo::ScreenToRay(
    const glm::vec2& mousePos,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    const glm::mat4& viewProjInv
) noexcept {
    if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
        return Ray{};
    }

    // Convert screen coordinates to NDC [-1, 1] for Vulkan coordinate system
    float xNdc = (2.0f * (mousePos.x - viewportPos.x) / viewportSize.x) - 1.0f;
    float yNdc = (2.0f * (mousePos.y - viewportPos.y) / viewportSize.y) - 1.0f;

    // Unproject near (z=0.0) and far (z=1.0) points
    glm::vec4 nearPointNdc(xNdc, yNdc, 0.0f, 1.0f);
    glm::vec4 farPointNdc(xNdc, yNdc, 1.0f, 1.0f);

    glm::vec4 nearWorld = viewProjInv * nearPointNdc;
    glm::vec4 farWorld = viewProjInv * farPointNdc;

    if (std::abs(nearWorld.w) < 1e-6f || std::abs(farWorld.w) < 1e-6f) {
        return Ray{};
    }

    glm::vec3 rayOrigin = glm::vec3(nearWorld) / nearWorld.w;
    glm::vec3 rayEnd = glm::vec3(farWorld) / farWorld.w;
    glm::vec3 rayDir = glm::normalize(rayEnd - rayOrigin);

    return Ray{ rayOrigin, rayDir };
}

glm::vec2 TransformGizmo::WorldToScreen(
    const glm::vec3& worldPos,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    const glm::mat4& viewProj,
    bool& outInFront
) noexcept {
    glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
    outInFront = (clipPos.w > 0.001f);

    if (!outInFront) {
        return glm::vec2(-1000.0f, -1000.0f);
    }

    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    float screenX = viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
    float screenY = viewportPos.y + (ndc.y * 0.5f + 0.5f) * viewportSize.y;

    return glm::vec2(screenX, screenY);
}

RayPlaneHit TransformGizmo::RayPlaneIntersection(
    const Ray& ray,
    const glm::vec3& planePoint,
    const glm::vec3& planeNormal
) noexcept {
    RayPlaneHit result{};
    float denom = glm::dot(ray.direction, planeNormal);

    if (std::abs(denom) < 1e-6f) {
        // Ray is parallel to plane
        result.hit = false;
        return result;
    }

    float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
    if (t < 0.0f) {
        // Intersection is behind ray origin
        result.hit = false;
        return result;
    }

    result.hit = true;
    result.distance = t;
    result.point = ray.origin + ray.direction * t;
    return result;
}

SkewRayHit TransformGizmo::RayAxisClosestPoint(
    const Ray& cursorRay,
    const glm::vec3& axisOrigin,
    const glm::vec3& axisDir
) noexcept {
    SkewRayHit result{};
    glm::vec3 u = glm::normalize(axisDir);
    glm::vec3 v = cursorRay.direction;
    glm::vec3 w0 = cursorRay.origin - axisOrigin;

    float a = glm::dot(v, v); // 1.0 since v is normalized
    float b = glm::dot(v, u);
    float c = glm::dot(u, u); // 1.0 since u is normalized
    float d = glm::dot(v, w0);
    float e = glm::dot(u, w0);

    float denom = a * c - b * b;
    if (denom < 1e-6f) {
        result.valid = false;
        return result;
    }

    float sc = (b * e - c * d) / denom; // parameter on cursorRay
    float tc = (a * e - b * d) / denom; // parameter on axis line

    result.valid = true;
    result.rayParam = sc;
    result.axisParam = tc;
    result.rayPoint = cursorRay.origin + v * sc;
    result.axisPoint = axisOrigin + u * tc;
    result.distance = glm::length(result.rayPoint - result.axisPoint);

    return result;
}

float TransformGizmo::SnapValue(float value, float snapStep) noexcept {
    if (snapStep <= 1e-5f) return value;
    return std::round(value / snapStep) * snapStep;
}

glm::vec3 TransformGizmo::SnapVector(const glm::vec3& vec, float snapStep) noexcept {
    return glm::vec3(
        SnapValue(vec.x, snapStep),
        SnapValue(vec.y, snapStep),
        SnapValue(vec.z, snapStep)
    );
}

float TransformGizmo::CalculateRotationAngle(
    const glm::vec3& startHit,
    const glm::vec3& currentHit,
    const glm::vec3& center,
    const glm::vec3& axisNormal
) noexcept {
    glm::vec3 v0 = startHit - center;
    glm::vec3 v1 = currentHit - center;

    float len0 = glm::length(v0);
    float len1 = glm::length(v1);
    if (len0 < 1e-5f || len1 < 1e-5f) return 0.0f;

    v0 /= len0;
    v1 /= len1;

    float cosAngle = std::clamp(glm::dot(v0, v1), -1.0f, 1.0f);
    glm::vec3 crossProd = glm::cross(v0, v1);
    float sinAngle = glm::dot(crossProd, glm::normalize(axisNormal));

    float angleRad = std::atan2(sinAngle, cosAngle);
    return glm::degrees(angleRad);
}

bool TransformGizmo::UpdateAndRender(
    ImDrawList* drawList,
    const Camera& camera,
    SceneNode* targetNode,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize
) {
    if (!targetNode || m_operation == GizmoOperation::None) {
        ResetInteraction();
        return false;
    }

    glm::mat4 worldMat = targetNode->GetWorldTransform();
    glm::vec3 centerWorld = glm::vec3(worldMat[3]);

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();
    glm::mat4 viewProjInv = glm::inverse(viewProj);

    bool inFront = false;
    glm::vec2 centerScreen = WorldToScreen(centerWorld, viewportPos, viewportSize, viewProj, inFront);
    if (!inFront) {
        return false;
    }

    // Determine axis directions based on Mode (World vs Local)
    glm::vec3 axisX(1.0f, 0.0f, 0.0f);
    glm::vec3 axisY(0.0f, 1.0f, 0.0f);
    glm::vec3 axisZ(0.0f, 0.0f, 1.0f);

    if (m_mode == GizmoMode::Local) {
        axisX = glm::normalize(glm::vec3(worldMat[0]));
        axisY = glm::normalize(glm::vec3(worldMat[1]));
        axisZ = glm::normalize(glm::vec3(worldMat[2]));
    }

    // Dispatch to specific operation renderer
    switch (m_operation) {
        case GizmoOperation::Translate:
            RenderTranslationGizmo(drawList, centerScreen, centerWorld, axisX, axisY, axisZ, viewProj, viewportPos, viewportSize);
            break;
        case GizmoOperation::Rotate:
            RenderRotationGizmo(drawList, centerScreen, centerWorld, axisX, axisY, axisZ, viewProj, viewportPos, viewportSize);
            break;
        case GizmoOperation::Scale:
            RenderScaleGizmo(drawList, centerScreen, centerWorld, axisX, axisY, axisZ, viewProj, viewportPos, viewportSize);
            break;
        case GizmoOperation::None:
            break;
    }

    // Handle mouse drag interaction
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    if (io.MouseDown[0]) {
        if (!m_isDragging && m_hoveredAxis != GizmoAxis::None) {
            m_isDragging = true;
            m_activeAxis = m_hoveredAxis;
            m_dragStartMousePos = mousePos;

            m_initialNodePosition = targetNode->position;
            m_initialNodeRotation = targetNode->rotationDegrees;
            m_initialNodeScale = targetNode->scale;

            Ray cursorRay = ScreenToRay(mousePos, viewportPos, viewportSize, viewProjInv);
            if (m_operation == GizmoOperation::Translate || m_operation == GizmoOperation::Scale) {
                glm::vec3 planeNorm = (m_activeAxis == GizmoAxis::X) ? axisY : ((m_activeAxis == GizmoAxis::Y) ? axisZ : axisX);
                auto hit = RayPlaneIntersection(cursorRay, centerWorld, planeNorm);
                m_dragStartHitPoint = hit.hit ? hit.point : centerWorld;
            } else if (m_operation == GizmoOperation::Rotate) {
                glm::vec3 rotAxis = (m_activeAxis == GizmoAxis::X) ? axisX : ((m_activeAxis == GizmoAxis::Y) ? axisY : axisZ);
                auto hit = RayPlaneIntersection(cursorRay, centerWorld, rotAxis);
                m_dragStartHitPoint = hit.hit ? hit.point : centerWorld;
            }
        }

        if (m_isDragging && m_activeAxis != GizmoAxis::None) {
            Ray cursorRay = ScreenToRay(mousePos, viewportPos, viewportSize, viewProjInv);

            if (m_operation == GizmoOperation::Translate) {
                glm::vec3 moveAxis = (m_activeAxis == GizmoAxis::X) ? axisX : ((m_activeAxis == GizmoAxis::Y) ? axisY : axisZ);
                auto closest = RayAxisClosestPoint(cursorRay, centerWorld, moveAxis);
                if (closest.valid) {
                    float deltaParam = closest.axisParam;
                    glm::vec3 newPos = m_initialNodePosition + moveAxis * deltaParam;
                    if (snapEnabled) {
                        newPos = SnapVector(newPos, translationSnap);
                    }
                    targetNode->position = newPos;
                    targetNode->SyncPropertiesToTransform();
                }
            } else if (m_operation == GizmoOperation::Rotate) {
                glm::vec3 rotAxis = (m_activeAxis == GizmoAxis::X) ? axisX : ((m_activeAxis == GizmoAxis::Y) ? axisY : axisZ);
                auto hit = RayPlaneIntersection(cursorRay, centerWorld, rotAxis);
                if (hit.hit) {
                    float deltaDeg = CalculateRotationAngle(m_dragStartHitPoint, hit.point, centerWorld, rotAxis);
                    if (snapEnabled) {
                        deltaDeg = SnapValue(deltaDeg, rotationSnap);
                    }
                    glm::vec3 newRot = m_initialNodeRotation;
                    if (m_activeAxis == GizmoAxis::X) newRot.x += deltaDeg;
                    else if (m_activeAxis == GizmoAxis::Y) newRot.y += deltaDeg;
                    else if (m_activeAxis == GizmoAxis::Z) newRot.z += deltaDeg;

                    targetNode->rotationDegrees = newRot;
                    targetNode->SyncPropertiesToTransform();
                }
            } else if (m_operation == GizmoOperation::Scale) {
                glm::vec3 scaleAxis = (m_activeAxis == GizmoAxis::X) ? axisX : ((m_activeAxis == GizmoAxis::Y) ? axisY : axisZ);
                auto closest = RayAxisClosestPoint(cursorRay, centerWorld, scaleAxis);
                if (closest.valid) {
                    float scaleFactor = 1.0f + (closest.axisParam * 0.1f);
                    scaleFactor = std::max(0.01f, scaleFactor);
                    if (snapEnabled) {
                        scaleFactor = SnapValue(scaleFactor, scaleSnap);
                    }
                    glm::vec3 newScale = m_initialNodeScale;
                    if (m_activeAxis == GizmoAxis::X) newScale.x *= scaleFactor;
                    else if (m_activeAxis == GizmoAxis::Y) newScale.y *= scaleFactor;
                    else if (m_activeAxis == GizmoAxis::Z) newScale.z *= scaleFactor;
                    else if (m_activeAxis == GizmoAxis::XYZ) newScale *= scaleFactor;

                    targetNode->scale = newScale;
                    targetNode->SyncPropertiesToTransform();
                }
            }
        }
    } else {
        if (m_isDragging) {
            m_isDragging = false;
            m_activeAxis = GizmoAxis::None;
        }
    }

    return m_isDragging;
}

void TransformGizmo::RenderTranslationGizmo(
    ImDrawList* drawList,
    const glm::vec2& centerScreen,
    const glm::vec3& centerWorld,
    const glm::vec3& axisX,
    const glm::vec3& axisY,
    const glm::vec3& axisZ,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize
) {
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    float axisLengthWorld = 1.5f;
    bool inFrontX = false, inFrontY = false, inFrontZ = false;

    glm::vec2 endX = WorldToScreen(centerWorld + axisX * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontX);
    glm::vec2 endY = WorldToScreen(centerWorld + axisY * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontY);
    glm::vec2 endZ = WorldToScreen(centerWorld + axisZ * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontZ);

    if (!m_isDragging) {
        m_hoveredAxis = GizmoAxis::None;
        if (glm::distance(mousePos, endX) <= handleRadius) m_hoveredAxis = GizmoAxis::X;
        else if (glm::distance(mousePos, endY) <= handleRadius) m_hoveredAxis = GizmoAxis::Y;
        else if (glm::distance(mousePos, endZ) <= handleRadius) m_hoveredAxis = GizmoAxis::Z;
    }

    ImU32 colX = (m_hoveredAxis == GizmoAxis::X || m_activeAxis == GizmoAxis::X) ? IM_COL32(255, 230, 0, 255) : IM_COL32(235, 60, 60, 255);
    ImU32 colY = (m_hoveredAxis == GizmoAxis::Y || m_activeAxis == GizmoAxis::Y) ? IM_COL32(255, 230, 0, 255) : IM_COL32(60, 220, 60, 255);
    ImU32 colZ = (m_hoveredAxis == GizmoAxis::Z || m_activeAxis == GizmoAxis::Z) ? IM_COL32(255, 230, 0, 255) : IM_COL32(60, 120, 245, 255);

    if (inFrontX) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endX.x, endX.y), colX, 3.0f);
        drawList->AddCircleFilled(ImVec2(endX.x, endX.y), 6.0f, colX);
    }
    if (inFrontY) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endY.x, endY.y), colY, 3.0f);
        drawList->AddCircleFilled(ImVec2(endY.x, endY.y), 6.0f, colY);
    }
    if (inFrontZ) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endZ.x, endZ.y), colZ, 3.0f);
        drawList->AddCircleFilled(ImVec2(endZ.x, endZ.y), 6.0f, colZ);
    }

    drawList->AddCircleFilled(ImVec2(centerScreen.x, centerScreen.y), 4.0f, IM_COL32(220, 220, 220, 255));
}

void TransformGizmo::RenderRotationGizmo(
    ImDrawList* drawList,
    const glm::vec2& centerScreen,
    const glm::vec3& centerWorld,
    const glm::vec3& axisX,
    const glm::vec3& axisY,
    const glm::vec3& axisZ,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize
) {
    if (!drawList) return;
    (void)centerScreen;

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    constexpr int segments = 36;
    float radiusWorld = 1.3f;

    auto drawRing = [&](const glm::vec3& u, const glm::vec3& v, GizmoAxis axisType, ImU32 defaultColor) {
        bool hovered = (!m_isDragging && m_hoveredAxis == axisType) || (m_isDragging && m_activeAxis == axisType);
        ImU32 col = hovered ? IM_COL32(255, 230, 0, 255) : defaultColor;

        ImVec2 prevPt;
        bool hasPrev = false;

        for (int i = 0; i <= segments; ++i) {
            float angle = (float(i) / float(segments)) * glm::two_pi<float>();
            glm::vec3 ptWorld = centerWorld + (u * std::cos(angle) + v * std::sin(angle)) * radiusWorld;
            bool inFront = false;
            glm::vec2 ptScreen = WorldToScreen(ptWorld, viewportPos, viewportSize, viewProj, inFront);

            if (inFront) {
                if (hasPrev) {
                    drawList->AddLine(prevPt, ImVec2(ptScreen.x, ptScreen.y), col, hovered ? 4.0f : 2.0f);
                }
                prevPt = ImVec2(ptScreen.x, ptScreen.y);
                hasPrev = true;

                if (!m_isDragging && glm::distance(mousePos, ptScreen) <= handleRadius) {
                    m_hoveredAxis = axisType;
                }
            } else {
                hasPrev = false;
            }
        }
    };

    if (!m_isDragging) m_hoveredAxis = GizmoAxis::None;

    drawRing(axisY, axisZ, GizmoAxis::X, IM_COL32(235, 60, 60, 220));
    drawRing(axisX, axisZ, GizmoAxis::Y, IM_COL32(60, 220, 60, 220));
    drawRing(axisX, axisY, GizmoAxis::Z, IM_COL32(60, 120, 245, 220));
}

void TransformGizmo::RenderScaleGizmo(
    ImDrawList* drawList,
    const glm::vec2& centerScreen,
    const glm::vec3& centerWorld,
    const glm::vec3& axisX,
    const glm::vec3& axisY,
    const glm::vec3& axisZ,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize
) {
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    float axisLengthWorld = 1.4f;
    bool inFrontX = false, inFrontY = false, inFrontZ = false;

    glm::vec2 endX = WorldToScreen(centerWorld + axisX * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontX);
    glm::vec2 endY = WorldToScreen(centerWorld + axisY * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontY);
    glm::vec2 endZ = WorldToScreen(centerWorld + axisZ * axisLengthWorld, viewportPos, viewportSize, viewProj, inFrontZ);

    if (!m_isDragging) {
        m_hoveredAxis = GizmoAxis::None;
        if (glm::distance(mousePos, endX) <= handleRadius) m_hoveredAxis = GizmoAxis::X;
        else if (glm::distance(mousePos, endY) <= handleRadius) m_hoveredAxis = GizmoAxis::Y;
        else if (glm::distance(mousePos, endZ) <= handleRadius) m_hoveredAxis = GizmoAxis::Z;
        else if (glm::distance(mousePos, centerScreen) <= handleRadius) m_hoveredAxis = GizmoAxis::XYZ;
    }

    ImU32 colX = (m_hoveredAxis == GizmoAxis::X || m_activeAxis == GizmoAxis::X) ? IM_COL32(255, 230, 0, 255) : IM_COL32(235, 60, 60, 255);
    ImU32 colY = (m_hoveredAxis == GizmoAxis::Y || m_activeAxis == GizmoAxis::Y) ? IM_COL32(255, 230, 0, 255) : IM_COL32(60, 220, 60, 255);
    ImU32 colZ = (m_hoveredAxis == GizmoAxis::Z || m_activeAxis == GizmoAxis::Z) ? IM_COL32(255, 230, 0, 255) : IM_COL32(60, 120, 245, 255);
    ImU32 colXYZ = (m_hoveredAxis == GizmoAxis::XYZ || m_activeAxis == GizmoAxis::XYZ) ? IM_COL32(255, 230, 0, 255) : IM_COL32(200, 200, 200, 255);

    if (inFrontX) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endX.x, endX.y), colX, 3.0f);
        drawList->AddRectFilled(ImVec2(endX.x - 5, endX.y - 5), ImVec2(endX.x + 5, endX.y + 5), colX);
    }
    if (inFrontY) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endY.x, endY.y), colY, 3.0f);
        drawList->AddRectFilled(ImVec2(endY.x - 5, endY.y - 5), ImVec2(endY.x + 5, endY.y + 5), colY);
    }
    if (inFrontZ) {
        drawList->AddLine(ImVec2(centerScreen.x, centerScreen.y), ImVec2(endZ.x, endZ.y), colZ, 3.0f);
        drawList->AddRectFilled(ImVec2(endZ.x - 5, endZ.y - 5), ImVec2(endZ.x + 5, endZ.y + 5), colZ);
    }

    drawList->AddRectFilled(ImVec2(centerScreen.x - 5, centerScreen.y - 5), ImVec2(centerScreen.x + 5, centerScreen.y + 5), colXYZ);
}

} // namespace khepri

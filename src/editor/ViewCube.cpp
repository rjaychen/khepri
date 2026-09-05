#include "ViewCube.h"
#include "../scene/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>

namespace khepri {

glm::vec3 ViewCube::GetNormalForFace(CubeFace face) noexcept {
    switch (face) {
        case CubeFace::Front:  return glm::vec3(0.0f, 0.0f, 1.0f);
        case CubeFace::Back:   return glm::vec3(0.0f, 0.0f, -1.0f);
        case CubeFace::Top:    return glm::vec3(0.0f, 1.0f, 0.0f);
        case CubeFace::Bottom: return glm::vec3(0.0f, -1.0f, 0.0f);
        case CubeFace::Right:  return glm::vec3(1.0f, 0.0f, 0.0f);
        case CubeFace::Left:   return glm::vec3(-1.0f, 0.0f, 0.0f);
        default:               return glm::vec3(0.0f);
    }
}

const char* ViewCube::GetFaceLabel(CubeFace face) noexcept {
    switch (face) {
        case CubeFace::Front:  return "FRONT";
        case CubeFace::Back:   return "BACK";
        case CubeFace::Top:    return "TOP";
        case CubeFace::Bottom: return "BOTTOM";
        case CubeFace::Right:  return "RIGHT";
        case CubeFace::Left:   return "LEFT";
        default:               return "";
    }
}

void ViewCube::GetYawPitchForDirection(ViewDirection dir, float& outYaw, float& outPitch) noexcept {
    switch (dir) {
        case ViewDirection::Front:
            outYaw = -90.0f; outPitch = 0.0f;
            break;
        case ViewDirection::Back:
            outYaw = 90.0f; outPitch = 0.0f;
            break;
        case ViewDirection::Right:
            outYaw = 180.0f; outPitch = 0.0f;
            break;
        case ViewDirection::Left:
            outYaw = 0.0f; outPitch = 0.0f;
            break;
        case ViewDirection::Top:
            outYaw = -90.0f; outPitch = 89.0f;
            break;
        case ViewDirection::Bottom:
            outYaw = -90.0f; outPitch = -89.0f;
            break;
        case ViewDirection::IsoTopFrontRight:
            outYaw = -135.0f; outPitch = 35.264f;
            break;
        case ViewDirection::IsoTopFrontLeft:
            outYaw = -45.0f; outPitch = 35.264f;
            break;
        case ViewDirection::IsoTopBackRight:
            outYaw = 135.0f; outPitch = 35.264f;
            break;
        case ViewDirection::IsoTopBackLeft:
            outYaw = 45.0f; outPitch = 35.264f;
            break;
    }
}

void ViewCube::GetYawPitchForFace(CubeFace face, float& outYaw, float& outPitch) noexcept {
    switch (face) {
        case CubeFace::Front:  GetYawPitchForDirection(ViewDirection::Front, outYaw, outPitch); break;
        case CubeFace::Back:   GetYawPitchForDirection(ViewDirection::Back, outYaw, outPitch); break;
        case CubeFace::Top:    GetYawPitchForDirection(ViewDirection::Top, outYaw, outPitch); break;
        case CubeFace::Bottom: GetYawPitchForDirection(ViewDirection::Bottom, outYaw, outPitch); break;
        case CubeFace::Right:  GetYawPitchForDirection(ViewDirection::Right, outYaw, outPitch); break;
        case CubeFace::Left:   GetYawPitchForDirection(ViewDirection::Left, outYaw, outPitch); break;
        default:               outYaw = -90.0f; outPitch = 0.0f; break;
    }
}

void ViewCube::GetYawPitchForCorner(CubeCorner corner, float& outYaw, float& outPitch) noexcept {
    switch (corner) {
        case CubeCorner::TopFrontRight:
            GetYawPitchForDirection(ViewDirection::IsoTopFrontRight, outYaw, outPitch);
            break;
        case CubeCorner::TopFrontLeft:
            GetYawPitchForDirection(ViewDirection::IsoTopFrontLeft, outYaw, outPitch);
            break;
        case CubeCorner::TopBackRight:
            GetYawPitchForDirection(ViewDirection::IsoTopBackRight, outYaw, outPitch);
            break;
        case CubeCorner::TopBackLeft:
            GetYawPitchForDirection(ViewDirection::IsoTopBackLeft, outYaw, outPitch);
            break;
        default:
            GetYawPitchForDirection(ViewDirection::IsoTopFrontRight, outYaw, outPitch);
            break;
    }
}

void ViewCube::SnapTo(Camera& camera, ViewDirection dir, bool animated) noexcept {
    float targetYaw = 0.0f, targetPitch = 0.0f;
    GetYawPitchForDirection(dir, targetYaw, targetPitch);

    if (!animated) {
        m_isAnimating = false;
        camera.SetOrientation(targetYaw, targetPitch);
        return;
    }

    m_startYaw = camera.GetYaw();
    m_startPitch = camera.GetPitch();

    // Shortest angular path for yaw
    while (targetYaw - m_startYaw > 180.0f) targetYaw -= 360.0f;
    while (targetYaw - m_startYaw < -180.0f) targetYaw += 360.0f;

    m_targetYaw = targetYaw;
    m_targetPitch = targetPitch;
    m_animTime = 0.0f;
    m_isAnimating = true;
}

void ViewCube::SnapToFace(Camera& camera, CubeFace face, bool animated) noexcept {
    float targetYaw = 0.0f, targetPitch = 0.0f;
    GetYawPitchForFace(face, targetYaw, targetPitch);

    if (!animated) {
        m_isAnimating = false;
        camera.SetOrientation(targetYaw, targetPitch);
        return;
    }

    m_startYaw = camera.GetYaw();
    m_startPitch = camera.GetPitch();

    while (targetYaw - m_startYaw > 180.0f) targetYaw -= 360.0f;
    while (targetYaw - m_startYaw < -180.0f) targetYaw += 360.0f;

    m_targetYaw = targetYaw;
    m_targetPitch = targetPitch;
    m_animTime = 0.0f;
    m_isAnimating = true;
}

void ViewCube::SnapToCorner(Camera& camera, CubeCorner corner, bool animated) noexcept {
    float targetYaw = 0.0f, targetPitch = 0.0f;
    GetYawPitchForCorner(corner, targetYaw, targetPitch);

    if (!animated) {
        m_isAnimating = false;
        camera.SetOrientation(targetYaw, targetPitch);
        return;
    }

    m_startYaw = camera.GetYaw();
    m_startPitch = camera.GetPitch();

    while (targetYaw - m_startYaw > 180.0f) targetYaw -= 360.0f;
    while (targetYaw - m_startYaw < -180.0f) targetYaw += 360.0f;

    m_targetYaw = targetYaw;
    m_targetPitch = targetPitch;
    m_animTime = 0.0f;
    m_isAnimating = true;
}

void ViewCube::Render(
    ImDrawList* drawList,
    Camera& camera,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    float deltaTime
) {
    if (!drawList) return;

    // 1. Process orientation transition animation
    if (m_isAnimating) {
        m_animTime += deltaTime;
        float progress = std::clamp(m_animTime / animDuration, 0.0f, 1.0f);
        // Smoothstep interpolation (3t^2 - 2t^3)
        float smoothT = progress * progress * (3.0f - 2.0f * progress);

        float curYaw = glm::mix(m_startYaw, m_targetYaw, smoothT);
        float curPitch = glm::mix(m_startPitch, m_targetPitch, smoothT);
        camera.SetOrientation(curYaw, curPitch);

        if (progress >= 1.0f) {
            m_isAnimating = false;
        }
    }

    // 2. ViewCube Screen Center Location (Top-Right)
    glm::vec2 cubeCenter = viewportPos + glm::vec2(viewportSize.x - cubeSize * 0.5f - margin, cubeSize * 0.5f + margin);

    // 3. Compute View Rotation Matrix
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    // Extract purely the 3x3 rotation part
    glm::mat3 rotMatrix = glm::mat3(viewMatrix);

    // 8 canonical vertices of a cube [-0.5, 0.5]
    static const glm::vec3 localVertices[8] = {
        {-0.5f, -0.5f,  0.5f}, // 0: Front-Bottom-Left
        { 0.5f, -0.5f,  0.5f}, // 1: Front-Bottom-Right
        { 0.5f,  0.5f,  0.5f}, // 2: Front-Top-Right
        {-0.5f,  0.5f,  0.5f}, // 3: Front-Top-Left
        {-0.5f, -0.5f, -0.5f}, // 4: Back-Bottom-Left
        { 0.5f, -0.5f, -0.5f}, // 5: Back-Bottom-Right
        { 0.5f,  0.5f, -0.5f}, // 6: Back-Top-Right
        {-0.5f,  0.5f, -0.5f}  // 7: Back-Top-Left
    };

    // Transform vertices to view space
    glm::vec3 rotatedVertices[8];
    glm::vec2 screenVertices[8];
    float halfSize = cubeSize * 0.45f;

    for (int i = 0; i < 8; ++i) {
        rotatedVertices[i] = rotMatrix * localVertices[i];
        // Screen projection: X is right, Y is down
        screenVertices[i] = cubeCenter + glm::vec2(rotatedVertices[i].x * halfSize, -rotatedVertices[i].y * halfSize);
    }

    struct FaceData {
        CubeFace face;
        int v[4];
        glm::vec3 normal;
        float depth;
    };

    FaceData faces[6] = {
        { CubeFace::Front,  {0, 1, 2, 3}, rotMatrix * glm::vec3( 0.0f,  0.0f,  1.0f), 0.0f },
        { CubeFace::Back,   {5, 4, 7, 6}, rotMatrix * glm::vec3( 0.0f,  0.0f, -1.0f), 0.0f },
        { CubeFace::Top,    {3, 2, 6, 7}, rotMatrix * glm::vec3( 0.0f,  1.0f,  0.0f), 0.0f },
        { CubeFace::Bottom, {4, 5, 1, 0}, rotMatrix * glm::vec3( 0.0f, -1.0f,  0.0f), 0.0f },
        { CubeFace::Right,  {1, 5, 6, 2}, rotMatrix * glm::vec3( 1.0f,  0.0f,  0.0f), 0.0f },
        { CubeFace::Left,   {4, 0, 3, 7}, rotMatrix * glm::vec3(-1.0f,  0.0f,  0.0f), 0.0f },
    };

    // Calculate depth for each face
    for (auto& f : faces) {
        f.depth = (rotatedVertices[f.v[0]].z + rotatedVertices[f.v[1]].z +
                   rotatedVertices[f.v[2]].z + rotatedVertices[f.v[3]].z) * 0.25f;
    }

    // Sort back-to-front (smaller z is deeper in camera space)
    std::sort(std::begin(faces), std::end(faces), [](const FaceData& a, const FaceData& b) {
        return a.depth < b.depth;
    });

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    m_hoveredFace = CubeFace::None;
    m_hoveredCorner = CubeCorner::None;

    // Helper point-in-polygon test for 4 vertices
    auto pointInQuad = [](const glm::vec2& p, const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
        auto cross = [](const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        };
        float c0 = cross(p0, p1, p);
        float c1 = cross(p1, p2, p);
        float c2 = cross(p2, p3, p);
        float c3 = cross(p3, p0, p);
        return (c0 >= 0 && c1 >= 0 && c2 >= 0 && c3 >= 0) || (c0 <= 0 && c1 <= 0 && c2 <= 0 && c3 <= 0);
    };

    // Check hovered face in front-to-back order
    for (int i = 5; i >= 0; --i) {
        const auto& f = faces[i];
        if (f.normal.z < 0.05f) continue; // Face pointing away or edge-on

        glm::vec2 p0 = screenVertices[f.v[0]];
        glm::vec2 p1 = screenVertices[f.v[1]];
        glm::vec2 p2 = screenVertices[f.v[2]];
        glm::vec2 p3 = screenVertices[f.v[3]];

        if (pointInQuad(mousePos, p0, p1, p2, p3)) {
            m_hoveredFace = f.face;
            break;
        }
    }

    // Render sorted visible faces
    for (const auto& f : faces) {
        if (f.normal.z < -0.05f) continue; // Skip backfaces

        glm::vec2 p0 = screenVertices[f.v[0]];
        glm::vec2 p1 = screenVertices[f.v[1]];
        glm::vec2 p2 = screenVertices[f.v[2]];
        glm::vec2 p3 = screenVertices[f.v[3]];

        bool isHovered = (m_hoveredFace == f.face);
        ImU32 fillColor = isHovered ? IM_COL32(80, 160, 240, 240) : IM_COL32(45, 50, 60, 210);
        ImU32 borderColor = isHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(110, 120, 140, 200);

        drawList->AddQuadFilled(
            ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
            ImVec2(p2.x, p2.y), ImVec2(p3.x, p3.y),
            fillColor
        );

        drawList->AddQuad(
            ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
            ImVec2(p2.x, p2.y), ImVec2(p3.x, p3.y),
            borderColor, 1.5f
        );

        // Draw face text label centered on the face quad
        glm::vec2 faceCenter = (p0 + p1 + p2 + p3) * 0.25f;
        const char* label = GetFaceLabel(f.face);
        ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddText(
            ImVec2(faceCenter.x - textSize.x * 0.5f, faceCenter.y - textSize.y * 0.5f),
            isHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 225, 230),
            label
        );
    }

    // Handle mouse click on face
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_hoveredFace != CubeFace::None) {
        SnapToFace(camera, m_hoveredFace, true);
    }
}

} // namespace khepri

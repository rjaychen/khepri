#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <string>

class Camera;

namespace khepri {

enum class CubeFace {
    Front = 0,
    Back,
    Top,
    Bottom,
    Left,
    Right,
    None
};

enum class CubeCorner {
    TopFrontRight = 0,
    TopFrontLeft,
    TopBackRight,
    TopBackLeft,
    BottomFrontRight,
    BottomFrontLeft,
    BottomBackRight,
    BottomBackLeft,
    None
};

enum class ViewDirection {
    Front = 0,
    Back,
    Top,
    Bottom,
    Left,
    Right,
    IsoTopFrontRight,
    IsoTopFrontLeft,
    IsoTopBackRight,
    IsoTopBackLeft
};

class ViewCube {
public:
    ViewCube() = default;
    ~ViewCube() = default;

    // -------------------------------------------------------------
    // Pure Mathematical Helpers & Lookups
    // -------------------------------------------------------------
    [[nodiscard]] static glm::vec3 GetNormalForFace(CubeFace face) noexcept;
    [[nodiscard]] static const char* GetFaceLabel(CubeFace face) noexcept;
    [[nodiscard]] static void GetYawPitchForDirection(ViewDirection dir, float& outYaw, float& outPitch) noexcept;
    [[nodiscard]] static void GetYawPitchForFace(CubeFace face, float& outYaw, float& outPitch) noexcept;
    [[nodiscard]] static void GetYawPitchForCorner(CubeCorner corner, float& outYaw, float& outPitch) noexcept;

    // -------------------------------------------------------------
    // ViewCube Configuration & State
    // -------------------------------------------------------------
    float cubeSize = 80.0f;       // Visual size in pixels
    float margin = 16.0f;         // Margin from top-right corner
    float animDuration = 0.25f;   // Seconds for smooth orientation animation

    [[nodiscard]] CubeFace GetHoveredFace() const noexcept { return m_hoveredFace; }
    [[nodiscard]] CubeCorner GetHoveredCorner() const noexcept { return m_hoveredCorner; }
    [[nodiscard]] bool IsHovered() const noexcept { return m_hoveredFace != CubeFace::None || m_hoveredCorner != CubeCorner::None; }
    [[nodiscard]] bool IsAnimating() const noexcept { return m_isAnimating; }

    // -------------------------------------------------------------
    // Update & Render Entry Point
    // -------------------------------------------------------------
    void Render(
        ImDrawList* drawList,
        Camera& camera,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        float deltaTime
    );

    void SnapTo(Camera& camera, ViewDirection dir, bool animated = true) noexcept;
    void SnapToFace(Camera& camera, CubeFace face, bool animated = true) noexcept;
    void SnapToCorner(Camera& camera, CubeCorner corner, bool animated = true) noexcept;

private:
    CubeFace m_hoveredFace = CubeFace::None;
    CubeCorner m_hoveredCorner = CubeCorner::None;

    // Smooth transition animation state
    bool m_isAnimating = false;
    float m_animTime = 0.0f;
    float m_startYaw = 0.0f;
    float m_targetYaw = 0.0f;
    float m_startPitch = 0.0f;
    float m_targetPitch = 0.0f;
};

} // namespace khepri

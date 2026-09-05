#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <vector>
#include <memory>
#include <string>

class Camera;
class SceneNode;
class LightComponent;

namespace khepri {

enum class LightHelperDisplayMode : int32_t {
    Selected = 0, // Draw helpers only for the currently selected light node
    All      = 1, // Draw helpers for all visible light nodes in the scene
    Hidden   = 2  // Hide all light helper visuals
};

struct VisualLine3D {
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
};

struct SpotConeGeometry {
    glm::vec3 apex{0.0f};
    glm::vec3 baseCenter{0.0f};
    float outerRadius = 0.0f;
    float innerRadius = 0.0f;
    std::vector<glm::vec3> outerCirclePoints;
    std::vector<glm::vec3> innerCirclePoints;
    std::vector<VisualLine3D> apexGeneratingLines;
    std::vector<VisualLine3D> innerGeneratingLines;
    VisualLine3D centerAxis;
};

struct DirectionalRaysGeometry {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    VisualLine3D centralRay;
    std::vector<VisualLine3D> centralArrowHead;
    std::vector<glm::vec3> baseRingPoints;
    std::vector<VisualLine3D> parallelRays;
    std::vector<VisualLine3D> rayArrowHeads;
};

struct PointLightRingsGeometry {
    glm::vec3 center{0.0f};
    float radius = 10.0f;
    std::vector<glm::vec3> xyRingPoints;
    std::vector<glm::vec3> xzRingPoints;
    std::vector<glm::vec3> yzRingPoints;
};

class LightVisualizer {
public:
    LightVisualizer() = default;
    ~LightVisualizer() = default;

    // -------------------------------------------------------------
    // Pure Mathematical Generators (Unit-Testable & Robust)
    // -------------------------------------------------------------

    [[nodiscard]] static SpotConeGeometry CalculateSpotConeGeometry(
        const glm::vec3& worldPos,
        const glm::vec3& worldDir,
        float range,
        float innerAngleDeg,
        float outerAngleDeg,
        uint32_t circleSegments = 32
    ) noexcept;

    [[nodiscard]] static DirectionalRaysGeometry CalculateDirectionalRaysGeometry(
        const glm::vec3& worldPos,
        const glm::vec3& worldDir,
        float ringRadius = 0.8f,
        float rayLength = 2.4f,
        uint32_t numRays = 4,
        uint32_t ringSegments = 24
    ) noexcept;

    [[nodiscard]] static PointLightRingsGeometry CalculatePointLightRingsGeometry(
        const glm::vec3& worldPos,
        float range,
        uint32_t segments = 36
    ) noexcept;

    // Screen projection with near-plane line clipping
    [[nodiscard]] static bool ProjectLineSegment(
        const glm::vec3& p1World,
        const glm::vec3& p2World,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        glm::vec2& outScreen1,
        glm::vec2& outScreen2
    ) noexcept;

    // -------------------------------------------------------------
    // Viewport Overlay Rendering
    // -------------------------------------------------------------

    static void RenderSceneLights(
        ImDrawList* drawList,
        const Camera& camera,
        const SceneNode* rootNode,
        const SceneNode* selectedNode,
        LightHelperDisplayMode displayMode,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize
    );

    static void RenderSingleLightHelper(
        ImDrawList* drawList,
        const Camera& camera,
        const SceneNode* node,
        const glm::mat4& worldTransform,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        bool isSelected
    );

private:
    static void Draw3DLine(
        ImDrawList* drawList,
        const glm::vec3& p1,
        const glm::vec3& p2,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ImU32 color,
        float thickness = 1.5f
    );

    static void Draw3DLoop(
        ImDrawList* drawList,
        const std::vector<glm::vec3>& loopPoints,
        const glm::mat4& viewProj,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ImU32 color,
        float thickness = 1.5f
    );
};

} // namespace khepri

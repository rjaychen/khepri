#include <gtest/gtest.h>
#include "editor/LightVisualizer.h"
#include "scene/LightComponent.h"
#include "scene/SceneNode.h"
#include "scene/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <imgui_internal.h>
#include <cmath>

using namespace khepri;

// ---------------------------------------------------------------------------
// 1. LightComponent Range & Properties Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, LightComponentRangeDefaultAndClamping) {
    LightComponent light(LightType::Point);
    EXPECT_FLOAT_EQ(light.GetRange(), 10.0f);

    light.SetRange(25.5f);
    EXPECT_FLOAT_EQ(light.GetRange(), 25.5f);

    // Clamping against non-positive / negative values
    light.SetRange(-10.0f);
    EXPECT_GE(light.GetRange(), 0.01f);

    light.SetRange(0.0f);
    EXPECT_GE(light.GetRange(), 0.01f);
}

// ---------------------------------------------------------------------------
// 2. Spot Cone Geometry Generation Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, SpotConeGeometryCalculations) {
    glm::vec3 worldPos(2.0f, 5.0f, -3.0f);
    glm::vec3 worldDir(0.0f, -1.0f, 0.0f); // Pointing straight down
    float range = 8.0f;
    float innerAngle = 15.0f;
    float outerAngle = 30.0f;
    uint32_t segments = 32;

    SpotConeGeometry cone = LightVisualizer::CalculateSpotConeGeometry(
        worldPos, worldDir, range, innerAngle, outerAngle, segments
    );

    // Apex should match world position
    EXPECT_FLOAT_EQ(cone.apex.x, worldPos.x);
    EXPECT_FLOAT_EQ(cone.apex.y, worldPos.y);
    EXPECT_FLOAT_EQ(cone.apex.z, worldPos.z);

    // Base center should be at worldPos + worldDir * range
    glm::vec3 expectedBase = worldPos + glm::vec3(0.0f, -range, 0.0f);
    EXPECT_NEAR(cone.baseCenter.x, expectedBase.x, 1e-4f);
    EXPECT_NEAR(cone.baseCenter.y, expectedBase.y, 1e-4f);
    EXPECT_NEAR(cone.baseCenter.z, expectedBase.z, 1e-4f);

    // Outer & Inner radius trigonometry verification
    float expectedOuterRadius = range * std::tan(glm::radians(outerAngle));
    float expectedInnerRadius = range * std::tan(glm::radians(innerAngle));
    EXPECT_NEAR(cone.outerRadius, expectedOuterRadius, 1e-4f);
    EXPECT_NEAR(cone.innerRadius, expectedInnerRadius, 1e-4f);
    EXPECT_GT(cone.outerRadius, cone.innerRadius);

    // Circle point counts
    EXPECT_EQ(cone.outerCirclePoints.size(), segments);
    EXPECT_EQ(cone.innerCirclePoints.size(), segments);

    // All outer circle points should be at outerRadius from baseCenter
    for (const auto& pt : cone.outerCirclePoints) {
        float d = glm::length(pt - cone.baseCenter);
        EXPECT_NEAR(d, cone.outerRadius, 1e-3f);
        // Base plane is y = expectedBase.y
        EXPECT_NEAR(pt.y, expectedBase.y, 1e-4f);
    }

    // All inner circle points should be at innerRadius from baseCenter
    for (const auto& pt : cone.innerCirclePoints) {
        float d = glm::length(pt - cone.baseCenter);
        EXPECT_NEAR(d, cone.innerRadius, 1e-3f);
        EXPECT_NEAR(pt.y, expectedBase.y, 1e-4f);
    }

    // 4 cardinal generating lines from apex
    EXPECT_EQ(cone.apexGeneratingLines.size(), 4u);
    for (const auto& line : cone.apexGeneratingLines) {
        EXPECT_FLOAT_EQ(line.start.x, worldPos.x);
        EXPECT_FLOAT_EQ(line.start.y, worldPos.y);
        EXPECT_FLOAT_EQ(line.start.z, worldPos.z);

        // Distance from base center to line end should match outer radius
        EXPECT_NEAR(glm::length(line.end - cone.baseCenter), cone.outerRadius, 1e-3f);
    }

    // Central axis line
    EXPECT_FLOAT_EQ(cone.centerAxis.start.x, worldPos.x);
    EXPECT_FLOAT_EQ(cone.centerAxis.start.y, worldPos.y);
    EXPECT_FLOAT_EQ(cone.centerAxis.start.z, worldPos.z);
    EXPECT_NEAR(cone.centerAxis.end.x, expectedBase.x, 1e-4f);
    EXPECT_NEAR(cone.centerAxis.end.y, expectedBase.y, 1e-4f);
    EXPECT_NEAR(cone.centerAxis.end.z, expectedBase.z, 1e-4f);
}

// ---------------------------------------------------------------------------
// 3. Directional Rays Geometry Generation Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, DirectionalRaysGeometryCalculations) {
    glm::vec3 worldPos(0.0f, 10.0f, 0.0f);
    glm::vec3 worldDir(1.0f, 0.0f, 0.0f); // Pointing +X
    float ringRadius = 1.0f;
    float rayLength = 3.0f;
    uint32_t numRays = 4;
    uint32_t ringSegments = 24;

    DirectionalRaysGeometry dirGeom = LightVisualizer::CalculateDirectionalRaysGeometry(
        worldPos, worldDir, ringRadius, rayLength, numRays, ringSegments
    );

    EXPECT_FLOAT_EQ(dirGeom.origin.x, worldPos.x);
    EXPECT_FLOAT_EQ(dirGeom.origin.y, worldPos.y);
    EXPECT_FLOAT_EQ(dirGeom.origin.z, worldPos.z);

    // Central ray verification
    EXPECT_FLOAT_EQ(dirGeom.centralRay.start.x, worldPos.x);
    EXPECT_FLOAT_EQ(dirGeom.centralRay.start.y, worldPos.y);
    EXPECT_FLOAT_EQ(dirGeom.centralRay.start.z, worldPos.z);

    glm::vec3 centralRayDelta = dirGeom.centralRay.end - dirGeom.centralRay.start;
    EXPECT_NEAR(glm::length(centralRayDelta), rayLength, 1e-4f);
    EXPECT_NEAR(glm::dot(glm::normalize(centralRayDelta), glm::vec3(1, 0, 0)), 1.0f, 1e-4f);

    // Base ring
    EXPECT_EQ(dirGeom.baseRingPoints.size(), ringSegments);
    for (const auto& pt : dirGeom.baseRingPoints) {
        float d = glm::length(pt - worldPos);
        EXPECT_NEAR(d, ringRadius, 1e-4f);
        // Plane perpendicular to +X is X = worldPos.x
        EXPECT_NEAR(pt.x, worldPos.x, 1e-4f);
    }

    // Parallel rays
    EXPECT_EQ(dirGeom.parallelRays.size(), numRays);
    for (const auto& ray : dirGeom.parallelRays) {
        // Starts on ring (X = worldPos.x, dist = ringRadius)
        EXPECT_NEAR(ray.start.x, worldPos.x, 1e-4f);
        EXPECT_NEAR(glm::length(ray.start - worldPos), ringRadius, 1e-3f);

        // Ray vector is strictly parallel to worldDir and has length rayLength
        glm::vec3 delta = ray.end - ray.start;
        EXPECT_NEAR(glm::length(delta), rayLength, 1e-4f);
        EXPECT_NEAR(glm::dot(glm::normalize(delta), glm::vec3(1, 0, 0)), 1.0f, 1e-4f);
    }

    // Central arrowhead
    EXPECT_FALSE(dirGeom.centralArrowHead.empty());
    // Ray arrowheads
    EXPECT_FALSE(dirGeom.rayArrowHeads.empty());
}

// ---------------------------------------------------------------------------
// 4. Point Light Range Rings Geometry Generation Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, PointLightRingsGeometryCalculations) {
    glm::vec3 worldPos(-4.0f, 2.0f, 6.0f);
    float range = 12.0f;
    uint32_t segments = 36;

    PointLightRingsGeometry ptGeom = LightVisualizer::CalculatePointLightRingsGeometry(
        worldPos, range, segments
    );

    EXPECT_FLOAT_EQ(ptGeom.center.x, worldPos.x);
    EXPECT_FLOAT_EQ(ptGeom.center.y, worldPos.y);
    EXPECT_FLOAT_EQ(ptGeom.center.z, worldPos.z);
    EXPECT_FLOAT_EQ(ptGeom.radius, range);

    EXPECT_EQ(ptGeom.xyRingPoints.size(), segments);
    EXPECT_EQ(ptGeom.xzRingPoints.size(), segments);
    EXPECT_EQ(ptGeom.yzRingPoints.size(), segments);

    // XY ring verification (Z remains constant)
    for (const auto& pt : ptGeom.xyRingPoints) {
        EXPECT_NEAR(pt.z, worldPos.z, 1e-4f);
        EXPECT_NEAR(glm::length(pt - worldPos), range, 1e-3f);
    }

    // XZ ring verification (Y remains constant)
    for (const auto& pt : ptGeom.xzRingPoints) {
        EXPECT_NEAR(pt.y, worldPos.y, 1e-4f);
        EXPECT_NEAR(glm::length(pt - worldPos), range, 1e-3f);
    }

    // YZ ring verification (X remains constant)
    for (const auto& pt : ptGeom.yzRingPoints) {
        EXPECT_NEAR(pt.x, worldPos.x, 1e-4f);
        EXPECT_NEAR(glm::length(pt - worldPos), range, 1e-3f);
    }
}

// ---------------------------------------------------------------------------
// 5. Line Segment Screen Projection & Near Plane Clipping Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, ProjectLineSegmentDirectProjection) {
    Camera camera(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    camera.SetViewportSize(1920.0f, 1080.0f);
    glm::mat4 viewProj = camera.GetViewProjectionMatrix();

    glm::vec2 viewportPos(100.0f, 50.0f);
    glm::vec2 viewportSize(1920.0f, 1080.0f);

    glm::vec3 p1(0.0f, 0.0f, 0.0f);
    glm::vec3 p2(1.0f, 1.0f, 0.0f);

    glm::vec2 s1, s2;
    bool visible = LightVisualizer::ProjectLineSegment(p1, p2, viewProj, viewportPos, viewportSize, s1, s2);
    EXPECT_TRUE(visible);

    // p1 is at center, so screen coord should be roughly center of viewport
    float expectedCenterX = viewportPos.x + viewportSize.x * 0.5f;
    float expectedCenterY = viewportPos.y + viewportSize.y * 0.5f;
    EXPECT_NEAR(s1.x, expectedCenterX, 2.0f);
    EXPECT_NEAR(s1.y, expectedCenterY, 2.0f);

    // With Camera's Vulkan projection, +X is right (s2.x > s1.x) and +Y is up / lower screen Y (s2.y < s1.y)
    EXPECT_GT(s2.x, s1.x);
    EXPECT_LT(s2.y, s1.y);
}

TEST(LightVisualizerTest, ProjectLineSegmentBehindCameraReturnsFalse) {
    Camera camera(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    camera.SetPerspective(60.0f, 1.0f, 0.1f, 100.0f);
    glm::mat4 viewProj = camera.GetViewProjectionMatrix();

    glm::vec2 viewportPos(0.0f, 0.0f);
    glm::vec2 viewportSize(800.0f, 600.0f);

    // Both points far behind camera (z = 25, 30 > camera z = 10 looking towards 0)
    glm::vec3 p1(0.0f, 0.0f, 25.0f);
    glm::vec3 p2(1.0f, 1.0f, 30.0f);

    glm::vec2 s1, s2;
    bool visible = LightVisualizer::ProjectLineSegment(p1, p2, viewProj, viewportPos, viewportSize, s1, s2);
    EXPECT_FALSE(visible);
}

TEST(LightVisualizerTest, ProjectLineSegmentStraddlingNearPlaneClipsGracefully) {
    Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    camera.SetPerspective(60.0f, 1.0f, 0.1f, 100.0f);
    glm::mat4 viewProj = camera.GetViewProjectionMatrix();

    glm::vec2 viewportPos(0.0f, 0.0f);
    glm::vec2 viewportSize(800.0f, 600.0f);

    // p1 is in front of camera (z = 0), p2 is behind camera (z = 10)
    glm::vec3 p1(0.0f, 0.0f, 0.0f);
    glm::vec3 p2(0.0f, 0.0f, 10.0f);

    glm::vec2 s1, s2;
    bool visible = LightVisualizer::ProjectLineSegment(p1, p2, viewProj, viewportPos, viewportSize, s1, s2);
    EXPECT_TRUE(visible);
    EXPECT_FALSE(std::isnan(s1.x) || std::isnan(s1.y) || std::isnan(s2.x) || std::isnan(s2.y));
    EXPECT_FALSE(std::isinf(s1.x) || std::isinf(s1.y) || std::isinf(s2.x) || std::isinf(s2.y));
}

TEST(LightVisualizerTest, LightGizmoSphereMeshGeometryIsSpherical) {
    VulkanContext* nullContext = nullptr;
    float radius = 0.35f;
    auto sphereMesh = MeshComponent::CreateSphere(nullContext, radius, 16, 8);
    ASSERT_NE(sphereMesh, nullptr);
    const auto& vertices = sphereMesh->GetVertices();
    EXPECT_GT(vertices.size(), 0u);

    // Every vertex should be at exactly the specified radius from the origin (spherical, not a cube)
    for (const auto& v : vertices) {
        float dist = glm::length(v.position);
        EXPECT_NEAR(dist, radius, 1e-4f);
    }
}

TEST(LightVisualizerTest, LightNodeGraphEvaluationDoesNotOverwriteLightMeshWithCube) {
    auto lightNode = std::make_unique<SceneNode>("Point Light");
    lightNode->lightComponent = std::make_shared<LightComponent>(LightType::Point);
    VulkanContext* nullContext = nullptr;
    auto initialSphere = MeshComponent::CreateSphere(nullContext, 0.35f, 16, 8);
    lightNode->mesh = initialSphere;

    // Trigger NodeGraph creation & evaluation
    auto graph = lightNode->GetOrCreateNodeGraph(nullContext);
    EXPECT_NE(graph, nullptr);
    lightNode->EvaluateNodeGraph(false);

    // The light node's mesh must remain the sphere mesh, and must NOT be replaced with a cube
    EXPECT_EQ(lightNode->mesh, initialSphere);
}

// ---------------------------------------------------------------------------
// 7. Light Billboard Icon & Screen Projection Tests
// ---------------------------------------------------------------------------

TEST(LightVisualizerTest, ProjectWorldToScreenInFrontAndBehindCamera) {
    Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();
    glm::vec2 viewportPos(100.0f, 50.0f);
    glm::vec2 viewportSize(800.0f, 600.0f);

    // Target point in front at origin
    glm::vec3 inFrontTarget(0.0f, 0.0f, 0.0f);
    glm::vec2 screenPos{0.0f};
    bool inFrontResult = LightVisualizer::ProjectWorldToScreen(inFrontTarget, viewProj, viewportPos, viewportSize, screenPos);
    EXPECT_TRUE(inFrontResult);
    // Center of viewport is 100 + 400 = 500, 50 + 300 = 350
    EXPECT_NEAR(screenPos.x, 500.0f, 2.0f);
    EXPECT_NEAR(screenPos.y, 350.0f, 2.0f);

    // Target point behind camera
    glm::vec3 behindTarget(0.0f, 0.0f, 15.0f);
    glm::vec2 behindScreen{0.0f};
    bool behindResult = LightVisualizer::ProjectWorldToScreen(behindTarget, viewProj, viewportPos, viewportSize, behindScreen);
    EXPECT_FALSE(behindResult);
}

TEST(LightVisualizerTest, LightIconHitTestingRadius) {
    glm::vec2 iconCenter(400.0f, 300.0f);
    float hitRadius = 18.0f;

    // Mouse directly on icon center
    EXPECT_LE(glm::distance(glm::vec2(400.0f, 300.0f), iconCenter), hitRadius);

    // Mouse near edge (12px away)
    EXPECT_LE(glm::distance(glm::vec2(400.0f, 312.0f), iconCenter), hitRadius);

    // Mouse just within radius (17.5px away)
    EXPECT_LE(glm::distance(glm::vec2(412.0f, 312.0f), iconCenter), hitRadius);

    // Mouse outside hit radius (25px away)
    EXPECT_GT(glm::distance(glm::vec2(400.0f, 325.0f), iconCenter), hitRadius);
    EXPECT_GT(glm::distance(glm::vec2(450.0f, 300.0f), iconCenter), hitRadius);
}

TEST(LightVisualizerTest, DrawLightIconAllTypesExecuteSafely) {
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    glm::vec2 screenCenter(200.0f, 150.0f);
    glm::vec3 lightColor(1.0f, 0.9f, 0.5f);

    // Test each light type with both selected and hover states
    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Directional, lightColor, false, false));
    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Directional, lightColor, true, true));

    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Point, lightColor, false, false));
    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Point, lightColor, true, true));

    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Spot, lightColor, false, false));
    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(drawList, screenCenter, LightType::Spot, lightColor, true, true));

    // Null drawlist safety
    EXPECT_NO_THROW(LightVisualizer::DrawLightIcon(nullptr, screenCenter, LightType::Point, lightColor, false, false));

    ImGui::EndFrame();
    ImGui::DestroyContext(ctx);
}


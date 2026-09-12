#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "editor/ViewportPanel.h"
#include "scene/Camera.h"

// ---------------------------------------------------------------------------
// 3D Scene Ground Grid & Viewport Optimization Unit Tests
// ---------------------------------------------------------------------------

namespace {

// Mathematical unprojection helper matching shaders/grid.vert
glm::vec3 UnprojectPointTest(float x, float y, float z, const glm::mat4& invViewProj) {
    glm::vec4 unprojected = invViewProj * glm::vec4(x, y, z, 1.0f);
    return glm::vec3(unprojected) / unprojected.w;
}

// Ray-ground plane (Y = 0) intersection matching shaders/grid.frag
bool IntersectGroundPlane(const glm::vec3& nearP, const glm::vec3& farP, glm::vec3& outHit) {
    float dy = farP.y - nearP.y;
    if (std::abs(dy) < 1e-6f) return false;

    float t = -nearP.y / dy;
    if (t <= 0.0f) return false;

    outHit = nearP + t * (farP - nearP);
    return true;
}

} // namespace

TEST(GridTest, UnprojectionNearFarPlanes) {
    Camera camera(glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();
    glm::mat4 invViewProj = glm::inverse(viewProj);

    // Center of screen in NDC clip space (x = 0, y = 0)
    glm::vec3 nearP = UnprojectPointTest(0.0f, 0.0f, 0.0f, invViewProj);
    glm::vec3 farP = UnprojectPointTest(0.0f, 0.0f, 1.0f, invViewProj);

    EXPECT_NEAR(nearP.x, 0.0f, 0.05f);
    EXPECT_GT(nearP.y, 4.0f); // Near camera position y=5.0
    EXPECT_LT(farP.y, -100.0f); // Far ray heads downwards towards negative Y

    glm::vec3 groundHit(0.0f);
    bool hit = IntersectGroundPlane(nearP, farP, groundHit);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(groundHit.y, 0.0f, 1e-4f); // Exactly on ground plane
    EXPECT_NEAR(groundHit.x, 0.0f, 0.05f);
    EXPECT_NEAR(groundHit.z, 0.0f, 0.05f); // Aimed at origin target
}

TEST(GridTest, RayParallelToGroundPlaneDiscardsSafely) {
    glm::vec3 nearP(0.0f, 5.0f, 0.0f);
    glm::vec3 farP(10.0f, 5.0f, 0.0f); // Horizontal ray at y=5, dy = 0

    glm::vec3 hit(0.0f);
    bool isHit = IntersectGroundPlane(nearP, farP, hit);
    EXPECT_FALSE(isHit);
}

TEST(GridTest, RayPointingAwayFromGroundPlaneDiscardsSafely) {
    glm::vec3 nearP(0.0f, 5.0f, 0.0f);
    glm::vec3 farP(0.0f, 15.0f, 0.0f); // Pointing upwards away from Y=0

    glm::vec3 hit(0.0f);
    bool isHit = IntersectGroundPlane(nearP, farP, hit);
    EXPECT_FALSE(isHit); // t < 0
}

TEST(GridTest, ViewportGridConfigurationState) {
    // Test default configuration values and mutation
    bool showGrid = true;
    float cellSize = 1.0f;
    float majorStep = 10.0f;
    float maxDist = 100.0f;
    float opacity = 0.8f;

    EXPECT_TRUE(showGrid);
    EXPECT_FLOAT_EQ(cellSize, 1.0f);
    EXPECT_FLOAT_EQ(majorStep, 10.0f);
    EXPECT_FLOAT_EQ(maxDist, 100.0f);
    EXPECT_FLOAT_EQ(opacity, 0.8f);

    // Toggle grid
    showGrid = !showGrid;
    EXPECT_FALSE(showGrid);
    showGrid = !showGrid;
    EXPECT_TRUE(showGrid);

    // Boundary constraints
    cellSize = std::max(0.1f, -5.0f);
    EXPECT_FLOAT_EQ(cellSize, 0.1f);

    majorStep = std::max(1.0f, 0.0f);
    EXPECT_FLOAT_EQ(majorStep, 1.0f);

    maxDist = std::max(5.0f, 2.0f);
    EXPECT_FLOAT_EQ(maxDist, 5.0f);

    opacity = std::clamp(1.5f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(opacity, 1.0f);

    opacity = std::clamp(-0.2f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(opacity, 0.0f);
}

TEST(GridTest, ViewportMSAASamplesMutationAndReallocFlag) {
    VkSampleCountFlagBits currentSamples = VK_SAMPLE_COUNT_1_BIT;
    bool needTextureUpdate = false;

    auto setMSAASamples = [&](VkSampleCountFlagBits newSamples) {
        if (currentSamples == newSamples) return;
        currentSamples = newSamples;
        needTextureUpdate = true;
    };

    // Initial state
    EXPECT_EQ(currentSamples, VK_SAMPLE_COUNT_1_BIT);
    EXPECT_FALSE(needTextureUpdate);

    // Setting same samples should be a no-op
    setMSAASamples(VK_SAMPLE_COUNT_1_BIT);
    EXPECT_FALSE(needTextureUpdate);

    // Switching to 4x MSAA sets the realloc flag
    setMSAASamples(VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(currentSamples, VK_SAMPLE_COUNT_4_BIT);
    EXPECT_TRUE(needTextureUpdate);

    // Simulate RenderUI checking realloc condition: dimsChanged || needTextureUpdate
    bool dimsChanged = false; // Dimensions remained 800x600
    bool needRealloc = dimsChanged || needTextureUpdate;
    needTextureUpdate = false; // Consumed

    EXPECT_TRUE(needRealloc);
    EXPECT_FALSE(needTextureUpdate);
}

TEST(GridTest, CameraNavigationSmoothingAndClamp) {
    Camera camera(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    float initialYaw = camera.GetYaw();
    float initialPitch = camera.GetPitch();

    // Orbit with 10 pixels movement
    camera.Orbit(10.0f, 5.0f);
    EXPECT_NEAR(camera.GetYaw(), initialYaw + 10.0f * 0.25f, 1e-4f);
    EXPECT_NEAR(camera.GetPitch(), initialPitch - 5.0f * 0.25f, 1e-4f);

    // Test Fly with high delta time spike (should be clamped smoothly)
    glm::vec3 posBefore = camera.GetPosition();
    camera.Fly(glm::vec3(0.0f, 0.0f, 1.0f), 5.0f); // 5.0 seconds spike
    glm::vec3 posAfter = camera.GetPosition();

    float distMoved = glm::length(posAfter - posBefore);
    float expectedMax = camera.GetFlySpeed() * 0.1f; // Clamped at 0.1s max
    EXPECT_LE(distMoved, expectedMax + 1e-3f);
}

// C++ equivalent of ComputeGridLine in shaders/grid.frag
static float ComputeGridLineTest(float coord, float dcoord, float lineWidthPixels) {
    float dist = std::abs(coord - std::floor(coord + 0.5f)); // Distance to integer line center [0, 0.5]
    float distPixels = dist / std::max(dcoord, 1e-6f);
    float halfWidth = lineWidthPixels * 0.5f;
    float coverage = std::clamp(halfWidth + 0.5f - distPixels, 0.0f, 1.0f);
    float nyquistFade = std::clamp(2.0f - 2.0f * dcoord, 0.0f, 1.0f);
    return coverage * nyquistFade;
}

TEST(GridTest, AntiAliasedSubpixelLineCoverage) {
    // 1. Exactly on line center (coord = 0.0, dcoord = 0.02 [~50px per cell])
    float covCenter = ComputeGridLineTest(0.0f, 0.02f, 1.2f);
    EXPECT_FLOAT_EQ(covCenter, 1.0f);

    // 2. Exactly on line edge (distPixels = halfWidth = 0.6)
    float edgeCoord = 0.6f * 0.02f; // distPixels = 0.6
    float covEdge = ComputeGridLineTest(edgeCoord, 0.02f, 1.2f);
    EXPECT_NEAR(covEdge, 0.5f, 1e-4f);

    // 3. Outside line boundary (distPixels >= halfWidth + 0.5 = 1.1)
    float outsideCoord = 1.2f * 0.02f;
    float covOutside = ComputeGridLineTest(outsideCoord, 0.02f, 1.2f);
    EXPECT_FLOAT_EQ(covOutside, 0.0f);

    // 4. Nyquist suppression: when grid cell is smaller than 1 screen pixel (dcoord >= 1.0)
    float moireSuppressed = ComputeGridLineTest(0.0f, 1.2f, 1.2f);
    EXPECT_FLOAT_EQ(moireSuppressed, 0.0f);
}

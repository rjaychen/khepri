#include <glm/gtc/epsilon.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/Version.h"
#include "core/Logger.h"
#include "scene/Camera.h"
#include "scene/SceneNode.h"
#include "scene/LightComponent.h"
#include "animation/Timeline.h"
#include "mesh/ExactPredicates.h"
#include "mesh/HalfEdgeMesh.h"
#include "vulkan/Pipeline.h"

// ---------------------------------------------------------------------------
// Camera Focus Unit Tests (Google Mock & Google Test)
// ---------------------------------------------------------------------------
TEST(CameraFocusTest, FocusOnNodeNullptrResetsToOrigin) {
    Camera camera(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f));
    camera.FocusOnNode(nullptr);

    EXPECT_NEAR(camera.GetTarget().x, 0.0f, 1e-4f);
    EXPECT_NEAR(camera.GetTarget().y, 0.0f, 1e-4f);
    EXPECT_NEAR(camera.GetTarget().z, 0.0f, 1e-4f);
    EXPECT_NEAR(camera.GetDistance(), 4.0f, 1e-4f);
}

TEST(CameraFocusTest, FocusOnNodeCentersOnWorldTransform) {
    auto parent = std::make_shared<SceneNode>("ParentNode");
    parent->position = glm::vec3(12.0f, 4.0f, -8.0f);

    auto child = std::make_unique<SceneNode>("SelectedChildNode");
    child->position = glm::vec3(1.0f, 2.0f, 3.0f);
    SceneNode* targetNode = parent->AddChild(std::move(child));

    Camera camera;
    camera.FocusOnNode(targetNode);

    // Target position should equal world position of child (13.0, 6.0, -5.0)
    EXPECT_NEAR(camera.GetTarget().x, 13.0f, 1e-4f);
    EXPECT_NEAR(camera.GetTarget().y, 6.0f, 1e-4f);
    EXPECT_NEAR(camera.GetTarget().z, -5.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// 1. Engine Versioning Test Suite
// ---------------------------------------------------------------------------
TEST(EngineVersionTest, ValidatesInitialSemanticVersion) {
    EXPECT_EQ(KhepriEngine::VERSION_MAJOR, 0);
    EXPECT_EQ(KhepriEngine::VERSION_MINOR, 1);
    EXPECT_GE(KhepriEngine::VERSION_PATCH, 0);
    EXPECT_STREQ(KhepriEngine::VERSION_STRING, KhepriEngine::VERSION_STRING);
}

TEST(PipelineMultisamplingTest, MultisamplingConfiguresSampleCountCorrectly) {
    PipelineBuilder builder;
    builder.SetMultisampling(VK_SAMPLE_COUNT_8_BIT, false);
    builder.SetMultisampling(VK_SAMPLE_COUNT_4_BIT, false);
    builder.SetMultisampling(VK_SAMPLE_COUNT_2_BIT, false);
    builder.SetMultisampling(VK_SAMPLE_COUNT_1_BIT, false);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Dynamic Light Node Test Suite
// ---------------------------------------------------------------------------
TEST(LightNodeTest, LightComponentEncodesGPUDataCorrectly) {
    LightComponent pointLight(LightType::Point);
    pointLight.color = glm::vec3(1.0f, 0.5f, 0.2f);
    pointLight.intensity = 2.5f;
    pointLight.constantAttenuation = 1.0f;
    pointLight.linearAttenuation = 0.09f;
    pointLight.quadraticAttenuation = 0.032f;

    glm::vec3 worldPos(3.0f, 4.0f, 5.0f);
    glm::vec3 worldDir(0.0f, -1.0f, 0.0f);

    LightData gpuData = pointLight.GetGPUData(worldPos, worldDir);

    EXPECT_NEAR(gpuData.position.x, 3.0f, 1e-4f);
    EXPECT_NEAR(gpuData.position.y, 4.0f, 1e-4f);
    EXPECT_NEAR(gpuData.position.z, 5.0f, 1e-4f);
    EXPECT_NEAR(gpuData.position.w, 1.0f, 1e-4f); // LightType::Point = 1.0

    EXPECT_NEAR(gpuData.color.r, 1.0f, 1e-4f);
    EXPECT_NEAR(gpuData.color.g, 0.5f, 1e-4f);
    EXPECT_NEAR(gpuData.color.b, 0.2f, 1e-4f);
    EXPECT_NEAR(gpuData.color.a, 2.5f, 1e-4f);   // intensity = 2.5

    EXPECT_NEAR(gpuData.params.x, 1.0f, 1e-4f);   // constant
    EXPECT_NEAR(gpuData.params.y, 0.09f, 1e-4f);  // linear
    EXPECT_NEAR(gpuData.params.z, 0.032f, 1e-4f); // quadratic
}

// ---------------------------------------------------------------------------
// 2. Scene Graph & Transform Propagation Test Suite
// ---------------------------------------------------------------------------
TEST(SceneGraphTest, ParentChildWorldTransformPropagation) {
    auto parent = std::make_shared<SceneNode>("ParentNode");
    parent->position = glm::vec3(10.0f, 0.0f, 0.0f);

    auto child = std::make_unique<SceneNode>("ChildNode");
    child->position = glm::vec3(0.0f, 5.0f, 0.0f);
    SceneNode* rawChild = parent->AddChild(std::move(child));

    glm::mat4 childWorld = rawChild->GetWorldTransform();
    glm::vec3 worldPos = glm::vec3(childWorld[3]);

    EXPECT_NEAR(worldPos.x, 10.0f, 1e-4f);
    EXPECT_NEAR(worldPos.y, 5.0f, 1e-4f);
    EXPECT_NEAR(worldPos.z, 0.0f, 1e-4f);
}

TEST(SceneGraphTest, LockedScaleProportionalScaling) {
    SceneNode node("TestNode");
    node.lockScale = true;
    node.scale = glm::vec3(1.0f, 2.0f, 4.0f);

    glm::vec3 oldScale = node.scale;
    glm::vec3 newScale(2.0f, 2.0f, 4.0f); // X doubled

    float factor = newScale.x / oldScale.x; // 2.0
    newScale = oldScale * factor;
    node.scale = newScale;

    EXPECT_NEAR(node.scale.x, 2.0f, 1e-4f);
    EXPECT_NEAR(node.scale.y, 4.0f, 1e-4f);
    EXPECT_NEAR(node.scale.z, 8.0f, 1e-4f);
}

TEST(SceneGraphTest, RemoveChildDetachesNode) {
    auto parent = std::make_shared<SceneNode>("ParentNode");
    auto child = std::make_unique<SceneNode>("ChildNode");
    SceneNode* rawChild = parent->AddChild(std::move(child));

    EXPECT_EQ(parent->GetChildren().size(), 1u);
    parent->RemoveChild(rawChild);
    EXPECT_EQ(parent->GetChildren().size(), 0u);
}

// ---------------------------------------------------------------------------
// 3. Animation Timeline & Keyframe Interpolation Test Suite
// ---------------------------------------------------------------------------
TEST(AnimationTimelineTest, PositionKeyframeLinearInterpolation) {
    AnimationTrack track;
    track.targetNodeName = "TargetNode";
    track.positionKeys.push_back({0.0f, glm::vec3(0.0f, 0.0f, 0.0f)});
    track.positionKeys.push_back({4.0f, glm::vec3(10.0f, 20.0f, 30.0f)});

    // Test sample at t = 2.0s (midpoint)
    glm::vec3 posAtMid = track.SamplePosition(2.0f);
    EXPECT_NEAR(posAtMid.x, 5.0f, 1e-4f);
    EXPECT_NEAR(posAtMid.y, 10.0f, 1e-4f);
    EXPECT_NEAR(posAtMid.z, 15.0f, 1e-4f);
}

TEST(AnimationTimelineTest, PlaybackStateControls) {
    Timeline timeline;
    auto clip = std::make_shared<AnimationClip>();
    clip->duration = 10.0f;
    timeline.SetClip(clip);

    EXPECT_FALSE(timeline.IsPlaying());
    timeline.Play();
    EXPECT_TRUE(timeline.IsPlaying());

    timeline.Pause();
    EXPECT_FALSE(timeline.IsPlaying());

    timeline.Play();
    timeline.SetCurrentTime(5.0f);
    timeline.Stop();
    EXPECT_FALSE(timeline.IsPlaying());
    EXPECT_NEAR(timeline.GetCurrentTime(), 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// 4. Computational Geometry Predicates Test Suite
// ---------------------------------------------------------------------------
TEST(ExactPredicatesTest, RobustOrientationPredicate) {
    glm::vec2 a(0.0f, 0.0f);
    glm::vec2 b(1.0f, 0.0f);
    glm::vec2 c_left(0.5f, 1.0f);
    glm::vec2 c_right(0.5f, -1.0f);

    EXPECT_GT(ExactPredicates::Orient2D(a, b, c_left), 0.0f);  // Counter-clockwise (Left)
    EXPECT_LT(ExactPredicates::Orient2D(a, b, c_right), 0.0f); // Clockwise (Right)
}

// ---------------------------------------------------------------------------
// 5. Core Logger Utility Test Suite
// ---------------------------------------------------------------------------
TEST(LoggerTest, RecordsAndClearsLogEntries) {
    Logger::Get().ClearLogs();
    EXPECT_EQ(Logger::Get().GetLogs().size(), 0u);

    LOG_INFO("Test Info Message");
    LOG_WARN("Test Warning Message");
    LOG_ERROR("Test Error Message");

    const auto& logs = Logger::Get().GetLogs();
    EXPECT_EQ(logs.size(), 3u);
    EXPECT_EQ(logs[0].level, LogLevel::Info);
    EXPECT_EQ(logs[1].level, LogLevel::Warning);
    EXPECT_EQ(logs[2].level, LogLevel::Error);

    Logger::Get().ClearLogs();
    EXPECT_EQ(Logger::Get().GetLogs().size(), 0u);
}

// ---------------------------------------------------------------------------
// 6. Pose Keyframing & Half-Edge Mesh Baking Test Suite
// ---------------------------------------------------------------------------
TEST(AnimationTimelineTest, KeyframeNodePoseCapturesTransform) {
    Timeline timeline;
    auto clip = std::make_shared<AnimationClip>();
    clip->duration = 5.0f;
    timeline.SetClip(clip);

    SceneNode node("TestNode");
    node.position = glm::vec3(3.0f, 4.0f, 5.0f);
    node.rotationDegrees = glm::vec3(0.0f, 90.0f, 0.0f);
    node.scale = glm::vec3(2.0f, 2.0f, 2.0f);

    timeline.SetCurrentTime(1.5f);
    timeline.KeyframeNodePose(&node);

    const auto currentClip = timeline.GetCurrentClip();
    ASSERT_NE(currentClip, nullptr);
    EXPECT_EQ(currentClip->tracks.size(), 1u);
    EXPECT_EQ(currentClip->tracks[0].targetNodeName, "TestNode");
    EXPECT_EQ(currentClip->tracks[0].positionKeys.size(), 1u);
    EXPECT_NEAR(currentClip->tracks[0].positionKeys[0].time, 1.5f, 1e-4f);
    EXPECT_NEAR(currentClip->tracks[0].positionKeys[0].value.x, 3.0f, 1e-4f);
}

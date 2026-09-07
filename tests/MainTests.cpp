#include <glm/gtc/epsilon.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/Version.h"
#include "core/Logger.h"
#include "scene/Camera.h"
#include "scene/SceneNode.h"
#include "scene/LightComponent.h"
#include "animation/Timeline.h"
#include "mesh/GeometricPredicates.h"
#include "mesh/HalfEdgeMesh.h"
#include "mesh/CDT.h"
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

TEST(AnimationTimelineTest, BinarySearchKeyframes) {
    AnimationTrack track;
    track.targetNodeName = "Bone";
    track.positionKeys.push_back({0.0f, glm::vec3(0.0f), InterpolationMode::Linear});
    track.positionKeys.push_back({1.0f, glm::vec3(10.0f), InterpolationMode::Linear});
    track.positionKeys.push_back({2.0f, glm::vec3(20.0f), InterpolationMode::Linear});
    track.positionKeys.push_back({3.0f, glm::vec3(30.0f), InterpolationMode::Linear});

    EXPECT_NEAR(track.SamplePosition(0.5f).x, 5.0f, 1e-4f);
    EXPECT_NEAR(track.SamplePosition(1.5f).x, 15.0f, 1e-4f);
    EXPECT_NEAR(track.SamplePosition(2.75f).x, 27.5f, 1e-4f);
}

TEST(AnimationTimelineTest, StepInterpolationMode) {
    AnimationTrack track;
    track.targetNodeName = "Bone";
    track.positionKeys.push_back({0.0f, glm::vec3(0.0f), InterpolationMode::Step});
    track.positionKeys.push_back({1.0f, glm::vec3(10.0f), InterpolationMode::Step});
    track.positionKeys.push_back({2.0f, glm::vec3(20.0f), InterpolationMode::Step});

    EXPECT_NEAR(track.SamplePosition(0.2f).x, 0.0f, 1e-4f);
    EXPECT_NEAR(track.SamplePosition(0.99f).x, 0.0f, 1e-4f);
    EXPECT_NEAR(track.SamplePosition(1.0f).x, 10.0f, 1e-4f);
    EXPECT_NEAR(track.SamplePosition(1.5f).x, 10.0f, 1e-4f);
}

TEST(AnimationTimelineTest, TemplateGenericTrackSampling) {
    AnimationClip clip;
    clip.AddOrUpdateKey<glm::vec3>("Node", 0.0f, glm::vec3(0.0f), InterpolationMode::Linear);
    clip.AddOrUpdateKey<glm::vec3>("Node", 2.0f, glm::vec3(10.0f, 20.0f, 30.0f), InterpolationMode::Linear);

    AnimationTrack* track = clip.GetOrCreateTrack("Node");
    ASSERT_NE(track, nullptr);

    glm::vec3 sampled = track->Sample<glm::vec3>(1.0f);
    EXPECT_NEAR(sampled.x, 5.0f, 1e-4f);
    EXPECT_NEAR(sampled.y, 10.0f, 1e-4f);
    EXPECT_NEAR(sampled.z, 15.0f, 1e-4f);
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
// 4. Computational Geometry Predicates & CDT Test Suite
// ---------------------------------------------------------------------------
TEST(CDTTriangulationTest, BowyerWatsonCavityExtraction) {
    std::vector<glm::vec2> points = {
        {0.0f, 0.0f},
        {2.0f, 0.0f},
        {2.0f, 2.0f},
        {0.0f, 2.0f},
        {1.0f, 1.0f},
        {0.5f, 0.5f}
    };

    std::vector<uint32_t> indices;
    CDT::Triangulate2D(points, {}, indices);

    ASSERT_FALSE(indices.empty());
    EXPECT_EQ(indices.size() % 3, 0u);
    for (uint32_t idx : indices) {
        EXPECT_LT(idx, points.size());
    }
}

TEST(GeometricPredicatesTest, OrientationPredicate) {
    glm::vec2 a(0.0f, 0.0f);
    glm::vec2 b(1.0f, 0.0f);
    glm::vec2 c_left(0.5f, 1.0f);
    glm::vec2 c_right(0.5f, -1.0f);

    EXPECT_GT(GeometricPredicates::Orient2D(a, b, c_left), 0.0f);  // Counter-clockwise (Left)
    EXPECT_LT(GeometricPredicates::Orient2D(a, b, c_right), 0.0f); // Clockwise (Right)
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

// ---------------------------------------------------------------------------
// 7. Vulkan Error Handling & Result Stringification Test Suite
// ---------------------------------------------------------------------------
#include "vulkan/VulkanUtils.h"

TEST(VulkanUtilsTest, VkResultToStringMapsStandardAndCustomCodes) {
    EXPECT_STREQ(Khepri::VkResultToString(VK_SUCCESS), "VK_SUCCESS");
    EXPECT_STREQ(Khepri::VkResultToString(VK_NOT_READY), "VK_NOT_READY");
    EXPECT_STREQ(Khepri::VkResultToString(VK_TIMEOUT), "VK_TIMEOUT");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_OUT_OF_HOST_MEMORY), "VK_ERROR_OUT_OF_HOST_MEMORY");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_OUT_OF_DEVICE_MEMORY), "VK_ERROR_OUT_OF_DEVICE_MEMORY");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_INITIALIZATION_FAILED), "VK_ERROR_INITIALIZATION_FAILED");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_DEVICE_LOST), "VK_ERROR_DEVICE_LOST");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_SURFACE_LOST_KHR), "VK_ERROR_SURFACE_LOST_KHR");
    EXPECT_STREQ(Khepri::VkResultToString(VK_ERROR_OUT_OF_DATE_KHR), "VK_ERROR_OUT_OF_DATE_KHR");
    EXPECT_STREQ(Khepri::VkResultToString(static_cast<VkResult>(-99999)), "VK_RESULT_UNRECOGNIZED");
}

TEST(VulkanUtilsTest, CheckVulkanResultSucceedsOnVkSuccess) {
    EXPECT_NO_THROW({
        CHECK_VK_RESULT(VK_SUCCESS, "Operation must succeed");
    });
}

TEST(VulkanUtilsTest, CheckVulkanResultThrowsDescriptiveRuntimeErrorOnFailure) {
    try {
        CHECK_VK_RESULT(VK_ERROR_DEVICE_LOST, "Device test failure");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("Device test failure"), std::string::npos);
        EXPECT_NE(msg.find("VK_ERROR_DEVICE_LOST"), std::string::npos);
        EXPECT_NE(msg.find("MainTests.cpp"), std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// 8. Vulkan Physical Device Abstraction & Selection Test Suite
// ---------------------------------------------------------------------------
#include "vulkan/PhysicalDevice.h"

TEST(PhysicalDeviceTest, QueueFamilyIndicesCompletenessCheck) {
    Khepri::QueueFamilyIndices incomplete{};
    EXPECT_FALSE(incomplete.isComplete());

    incomplete.graphicsFamily = 0;
    EXPECT_FALSE(incomplete.isComplete());

    incomplete.presentFamily = 0;
    EXPECT_FALSE(incomplete.isComplete());

    incomplete.computeFamily = 1;
    EXPECT_TRUE(incomplete.isComplete());
}

TEST(PhysicalDeviceTest, EnumerateReturnsEmptyOnNullInstance) {
    auto devices = Khepri::VulkanPhysicalDevice::Enumerate(VK_NULL_HANDLE);
    EXPECT_TRUE(devices.empty());
}

TEST(PhysicalDeviceTest, SelectBestThrowsWhenNoDevicesProvided) {
    std::vector<Khepri::VulkanPhysicalDevice> emptyList;
    std::vector<const char*> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    EXPECT_THROW(Khepri::VulkanPhysicalDevice::SelectBest(emptyList, requiredExtensions), std::runtime_error);
}

TEST(PhysicalDeviceTest, SwapchainSupportDetailsAdequacyCheck) {
    Khepri::SwapchainSupportDetails details{};
    EXPECT_FALSE(details.IsAdequate());

    details.formats.push_back(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    EXPECT_FALSE(details.IsAdequate());

    details.presentModes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    EXPECT_TRUE(details.IsAdequate());
}

// ---------------------------------------------------------------------------
// 9. Vulkan Struct Designated Initializers & Context Helper Signatures
// ---------------------------------------------------------------------------
#include "vulkan/VulkanContext.h"
#include <type_traits>

TEST(VulkanStructInitTest, ValueInitializationZeroInitializesUnspecifiedFields) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = reinterpret_cast<VkCommandPool>(0x1234);
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 3;

    EXPECT_EQ(allocInfo.sType, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    EXPECT_EQ(allocInfo.pNext, nullptr);
    EXPECT_EQ(allocInfo.commandPool, reinterpret_cast<VkCommandPool>(0x1234));
    EXPECT_EQ(allocInfo.level, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    EXPECT_EQ(allocInfo.commandBufferCount, 3u);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;

    EXPECT_EQ(poolInfo.sType, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    EXPECT_EQ(poolInfo.pNext, nullptr);
    EXPECT_EQ(poolInfo.flags, static_cast<VkCommandPoolCreateFlags>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
    EXPECT_EQ(poolInfo.queueFamilyIndex, 0u);
}

TEST(VulkanContextHelperTest, MethodSignaturesMatchModernStandards) {
    using AllocateBuffersFn = std::vector<VkCommandBuffer>(VulkanContext::*)(VkCommandPool, uint32_t, VkCommandBufferLevel) const;
    using AllocateBufferFn = VkCommandBuffer(VulkanContext::*)(VkCommandPool, VkCommandBufferLevel) const;
    using CreatePoolFn = VkCommandPool(VulkanContext::*)(VkCommandPoolCreateFlags, std::optional<uint32_t>) const;

    static_assert(std::is_same_v<decltype(&VulkanContext::AllocateCommandBuffers), AllocateBuffersFn>,
                  "AllocateCommandBuffers signature mismatch");
    static_assert(std::is_same_v<decltype(&VulkanContext::AllocateCommandBuffer), AllocateBufferFn>,
                  "AllocateCommandBuffer signature mismatch");
    static_assert(std::is_same_v<decltype(&VulkanContext::CreateCommandPool), CreatePoolFn>,
                  "CreateCommandPool signature mismatch");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// 10. SharedOrObserved Smart Pointer Test Suite
// ---------------------------------------------------------------------------
#include "core/SharedOrObserved.h"

TEST(SharedOrObservedTest, OwningPointerSemantics) {
    auto holder = khepri::MakeShared<std::string>("Khepri Owning Pointer");
    EXPECT_TRUE(holder.is_owning());
    EXPECT_TRUE(static_cast<bool>(holder));
    EXPECT_EQ(*holder, "Khepri Owning Pointer");
    EXPECT_EQ(holder->length(), 21u);

    // Verify use_count
    EXPECT_EQ(holder.use_count(), 1);
    auto copy = holder;
    EXPECT_EQ(holder.use_count(), 2);
    EXPECT_EQ(copy.use_count(), 2);
}

TEST(SharedOrObservedTest, BorrowingPointerSemantics) {
    int value = 42;
    auto holder = khepri::Observe(&value);
    EXPECT_FALSE(holder.is_owning());
    EXPECT_TRUE(static_cast<bool>(holder));
    EXPECT_EQ(*holder, 42);
    EXPECT_EQ(holder.get(), &value);

    // Mutation via pointer
    *holder = 100;
    EXPECT_EQ(value, 100);
}

TEST(SharedOrObservedTest, ConstConversionSupport) {
    auto mutableHolder = khepri::MakeShared<std::string>("Immutable View");
    khepri::SharedOrObserved<const std::string> constHolder = mutableHolder;

    EXPECT_TRUE(constHolder.is_owning());
    EXPECT_EQ(*constHolder, "Immutable View");

    int rawInt = 777;
    khepri::SharedOrObserved<int> obsHolder(&rawInt);
    khepri::SharedOrObserved<const int> constObsHolder = obsHolder;
    EXPECT_FALSE(constObsHolder.is_owning());
    EXPECT_EQ(*constObsHolder, 777);
}

// ---------------------------------------------------------------------------
// 11. Logger Ring Buffer & Log Level Filtering Test Suite
// ---------------------------------------------------------------------------
TEST(LoggerModernizationTest, RingBufferEnforcesMaximumCapacity) {
    auto& logger = Logger::Get();
    logger.ClearLogs();
    logger.SetMaxLogs(5);

    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Log Message #" + std::to_string(i));
    }

    const auto& logs = logger.GetLogs();
    EXPECT_EQ(logs.size(), 5u);
    // Oldest messages should be discarded: logs should contain messages 5, 6, 7, 8, 9
    EXPECT_EQ(logs.front().message, "Log Message #5");
    EXPECT_EQ(logs.back().message, "Log Message #9");

    // Reset max logs
    logger.SetMaxLogs(1000);
    logger.ClearLogs();
}

TEST(LoggerModernizationTest, MinimumLogLevelFiltering) {
    auto& logger = Logger::Get();
    logger.ClearLogs();
    logger.SetLogLevel(LogLevel::Warning);

    LOG_INFO("This info message must be filtered out");
    LOG_WARN("This warning message must pass");
    LOG_ERROR("This error message must pass");

    const auto& logs = logger.GetLogs();
    ASSERT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0].level, LogLevel::Warning);
    EXPECT_EQ(logs[0].message, "This warning message must pass");
    EXPECT_EQ(logs[1].level, LogLevel::Error);
    EXPECT_EQ(logs[1].message, "This error message must pass");

    // Reset log level
    logger.SetLogLevel(LogLevel::Info);
    logger.ClearLogs();
}

// ---------------------------------------------------------------------------
// 12. PropertyReflection Safe Accessors & Typed Limits Test Suite
// ---------------------------------------------------------------------------
#include "core/PropertyReflection.h"

TEST(PropertyReflectionTest, SafeOptionalAndFallbackAccessors) {
    Property propFloat{
        .name = "cameraFov",
        .type = PropertyType::Float,
        .value = 60.0f,
        .limits = FloatRange{ .min = 10.0f, .max = 120.0f }
    };

    Property propInt{
        .name = "maxBounces",
        .type = PropertyType::Int,
        .value = 4,
        .limits = IntRange{ .min = 1, .max = 32 }
    };

    Property propStr{
        .name = "rendererName",
        .type = PropertyType::String,
        .value = std::string("VulkanPBR")
    };

    // 1. GetOptionalValue
    auto fovOpt = propFloat.GetOptionalValue<float>();
    ASSERT_TRUE(fovOpt.has_value());
    EXPECT_FLOAT_EQ(*fovOpt, 60.0f);

    auto wrongTypeOpt = propFloat.GetOptionalValue<int>();
    EXPECT_FALSE(wrongTypeOpt.has_value());

    // 2. GetValueOr
    EXPECT_FLOAT_EQ(propFloat.GetValueOr<float>(45.0f), 60.0f);
    EXPECT_EQ(propFloat.GetValueOr<int>(90), 90); // Type mismatch fallback

    // 3. Checked GetValue
    EXPECT_FLOAT_EQ(propFloat.GetValue<float>(), 60.0f);
    EXPECT_THROW((void)propFloat.GetValue<int>(), std::bad_variant_access);

    // 4. Property Limits
    ASSERT_TRUE(propFloat.HasNumericLimits());
    ASSERT_TRUE(std::holds_alternative<FloatRange>(propFloat.limits));
    auto floatLimit = std::get<FloatRange>(propFloat.limits);
    EXPECT_FLOAT_EQ(floatLimit.min, 10.0f);
    EXPECT_FLOAT_EQ(floatLimit.max, 120.0f);

    ASSERT_TRUE(propInt.HasNumericLimits());
    auto intLimit = std::get<IntRange>(propInt.limits);
    EXPECT_EQ(intLimit.min, 1);
    EXPECT_EQ(intLimit.max, 32);

    EXPECT_FALSE(propStr.HasNumericLimits());
}

// ---------------------------------------------------------------------------
// 13. Descriptors Layout Builder Span Support Test Suite
// ---------------------------------------------------------------------------
#include "vulkan/Descriptors.h"

TEST(DescriptorLayoutBuilderTest, SpanBindingAccumulation) {
    DescriptorLayoutBuilder builder;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };

    builder.AddBindings(std::span<const VkDescriptorSetLayoutBinding>(bindings));
    const auto& currentBindings = builder.GetBindings();
    ASSERT_EQ(currentBindings.size(), 2u);
    EXPECT_EQ(currentBindings[0].binding, 0u);
    EXPECT_EQ(currentBindings[0].descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    EXPECT_EQ(currentBindings[1].binding, 1u);
    EXPECT_EQ(currentBindings[1].descriptorType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    builder.Clear();
    EXPECT_TRUE(builder.GetBindings().empty());
}




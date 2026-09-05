#include <gtest/gtest.h>
#include "editor/TransformGizmo.h"
#include "scene/SceneNode.h"
#include "scene/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace khepri;

TEST(TransformGizmoTest, RayPlaneIntersectionDirectHit) {
    Ray ray{ glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
    glm::vec3 planePoint(0.0f, 0.0f, 0.0f);
    glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);

    auto hit = TransformGizmo::RayPlaneIntersection(ray, planePoint, planeNormal);
    EXPECT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.distance, 5.0f);
    EXPECT_NEAR(hit.point.x, 0.0f, 1e-5f);
    EXPECT_NEAR(hit.point.y, 0.0f, 1e-5f);
    EXPECT_NEAR(hit.point.z, 0.0f, 1e-5f);
}

TEST(TransformGizmoTest, RayPlaneIntersectionParallelReturnsFalse) {
    Ray ray{ glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f) };
    glm::vec3 planePoint(0.0f, 0.0f, 0.0f);
    glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);

    auto hit = TransformGizmo::RayPlaneIntersection(ray, planePoint, planeNormal);
    EXPECT_FALSE(hit.hit);
}

TEST(TransformGizmoTest, RayPlaneIntersectionBehindReturnsFalse) {
    Ray ray{ glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
    glm::vec3 planePoint(0.0f, 0.0f, 0.0f);
    glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);

    auto hit = TransformGizmo::RayPlaneIntersection(ray, planePoint, planeNormal);
    EXPECT_FALSE(hit.hit);
}

TEST(TransformGizmoTest, RayAxisClosestPointOrthogonal) {
    Ray cursorRay{ glm::vec3(3.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
    glm::vec3 axisOrigin(0.0f, 0.0f, 0.0f);
    glm::vec3 axisDir(1.0f, 0.0f, 0.0f); // X-axis

    auto hit = TransformGizmo::RayAxisClosestPoint(cursorRay, axisOrigin, axisDir);
    EXPECT_TRUE(hit.valid);
    EXPECT_NEAR(hit.axisParam, 3.0f, 1e-5f);
    EXPECT_NEAR(hit.rayParam, 5.0f, 1e-5f);
    EXPECT_NEAR(hit.axisPoint.x, 3.0f, 1e-5f);
    EXPECT_NEAR(hit.axisPoint.y, 0.0f, 1e-5f);
    EXPECT_NEAR(hit.axisPoint.z, 0.0f, 1e-5f);
    EXPECT_NEAR(hit.rayPoint.x, 3.0f, 1e-5f);
    EXPECT_NEAR(hit.rayPoint.y, 2.0f, 1e-5f);
    EXPECT_NEAR(hit.rayPoint.z, 0.0f, 1e-5f);
    EXPECT_NEAR(hit.distance, 2.0f, 1e-5f);
}

TEST(TransformGizmoTest, SnapValueAndSnapVectorQuantization) {
    EXPECT_NEAR(TransformGizmo::SnapValue(1.35f, 0.5f), 1.5f, 1e-5f);
    EXPECT_NEAR(TransformGizmo::SnapValue(1.20f, 0.5f), 1.0f, 1e-5f);
    EXPECT_NEAR(TransformGizmo::SnapValue(14.2f, 15.0f), 15.0f, 1e-5f);
    EXPECT_NEAR(TransformGizmo::SnapValue(7.0f, 15.0f), 0.0f, 1e-5f);

    glm::vec3 rawVec(1.3f, 4.8f, -0.4f);
    glm::vec3 snapped = TransformGizmo::SnapVector(rawVec, 1.0f);
    EXPECT_NEAR(snapped.x, 1.0f, 1e-5f);
    EXPECT_NEAR(snapped.y, 5.0f, 1e-5f);
    EXPECT_NEAR(snapped.z, 0.0f, 1e-5f);
}

TEST(TransformGizmoTest, CalculateRotationAngleQuarterTurn) {
    glm::vec3 center(0.0f);
    glm::vec3 axisNormal(0.0f, 1.0f, 0.0f); // Y-axis rotation
    glm::vec3 startHit(1.0f, 0.0f, 0.0f);
    glm::vec3 currentHit(0.0f, 0.0f, -1.0f);

    float angleDeg = TransformGizmo::CalculateRotationAngle(startHit, currentHit, center, axisNormal);
    EXPECT_NEAR(std::abs(angleDeg), 90.0f, 1e-3f);
}

TEST(TransformGizmoTest, GizmoOperationAndModeStateTransitions) {
    TransformGizmo gizmo;
    EXPECT_EQ(gizmo.GetOperation(), GizmoOperation::Translate);
    EXPECT_EQ(gizmo.GetMode(), GizmoMode::World);

    gizmo.SetOperation(GizmoOperation::Rotate);
    EXPECT_EQ(gizmo.GetOperation(), GizmoOperation::Rotate);

    gizmo.SetOperation(GizmoOperation::Scale);
    EXPECT_EQ(gizmo.GetOperation(), GizmoOperation::Scale);

    gizmo.SetMode(GizmoMode::Local);
    EXPECT_EQ(gizmo.GetMode(), GizmoMode::Local);

    EXPECT_FALSE(gizmo.IsUsing());
    EXPECT_FALSE(gizmo.IsHovered());
}

TEST(TransformGizmoTest, SceneNodeTransformSyncAndWorldMatrixUpdate) {
    SceneNode node("TestNode");
    node.position = glm::vec3(5.0f, 10.0f, -3.0f);
    node.rotationDegrees = glm::vec3(0.0f, 90.0f, 0.0f);
    node.scale = glm::vec3(2.0f, 2.0f, 2.0f);

    node.SyncPropertiesToTransform();
    glm::mat4 world = node.GetWorldTransform();

    // Translation component
    EXPECT_NEAR(world[3][0], 5.0f, 1e-4f);
    EXPECT_NEAR(world[3][1], 10.0f, 1e-4f);
    EXPECT_NEAR(world[3][2], -3.0f, 1e-4f);

    // X-axis rotated 90 deg around Y should point towards -Z or +Z depending on handedness
    glm::vec3 transformedX = glm::vec3(world * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_NEAR(glm::length(transformedX), 2.0f, 1e-4f); // scaled by 2
}

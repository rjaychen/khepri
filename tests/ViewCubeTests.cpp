#include <gtest/gtest.h>
#include "editor/ViewCube.h"
#include "scene/Camera.h"

using namespace khepri;

TEST(ViewCubeTest, GetNormalForFaceMatchesGeometry) {
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Front),  glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Back),   glm::vec3(0.0f, 0.0f, -1.0f));
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Top),    glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Bottom), glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Right),  glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(ViewCube::GetNormalForFace(CubeFace::Left),   glm::vec3(-1.0f, 0.0f, 0.0f));
}

TEST(ViewCubeTest, FaceLabelsMatchSpecification) {
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Front),  "FRONT");
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Back),   "BACK");
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Top),    "TOP");
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Bottom), "BOTTOM");
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Right),  "RIGHT");
    EXPECT_STREQ(ViewCube::GetFaceLabel(CubeFace::Left),   "LEFT");
}

TEST(ViewCubeTest, GetYawPitchForDirectionProducesCorrectAngles) {
    float yaw = 0.0f, pitch = 0.0f;

    ViewCube::GetYawPitchForDirection(ViewDirection::Front, yaw, pitch);
    EXPECT_FLOAT_EQ(yaw, -90.0f);
    EXPECT_FLOAT_EQ(pitch, 0.0f);

    ViewCube::GetYawPitchForDirection(ViewDirection::Back, yaw, pitch);
    EXPECT_FLOAT_EQ(yaw, 90.0f);
    EXPECT_FLOAT_EQ(pitch, 0.0f);

    ViewCube::GetYawPitchForDirection(ViewDirection::Top, yaw, pitch);
    EXPECT_FLOAT_EQ(yaw, -90.0f);
    EXPECT_FLOAT_EQ(pitch, 89.0f);

    ViewCube::GetYawPitchForDirection(ViewDirection::Bottom, yaw, pitch);
    EXPECT_FLOAT_EQ(yaw, -90.0f);
    EXPECT_FLOAT_EQ(pitch, -89.0f);
}

TEST(ViewCubeTest, SnapToFaceUpdatesCameraOrientationInstantlyWhenNotAnimated) {
    Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f));
    ViewCube viewCube;

    viewCube.SnapToFace(camera, CubeFace::Top, false);
    EXPECT_FLOAT_EQ(camera.GetYaw(), -90.0f);
    EXPECT_FLOAT_EQ(camera.GetPitch(), 89.0f);

    viewCube.SnapToFace(camera, CubeFace::Right, false);
    EXPECT_FLOAT_EQ(camera.GetYaw(), 180.0f);
    EXPECT_FLOAT_EQ(camera.GetPitch(), 0.0f);
}

#include <gtest/gtest.h>
#include "../src/scene/LightNode.h"
#include "../src/scene/LightComponent.h"
#include "../src/scene/SceneNode.h"
#include <memory>

// ---------------------------------------------------------------------------
// SOLID Light Architecture Tests (Unreal/Unity Style)
// ---------------------------------------------------------------------------

TEST(LightNodeTest, DirectionalLightNodeInitializationAndProperties) {
    DirectionalLightNode dirLight(
        "Sun Light",
        glm::vec3(5.0f, 10.0f, 5.0f),
        glm::vec3(-45.0f, 45.0f, 0.0f),
        glm::vec3(1.0f, 0.95f, 0.85f),
        1.5f
    );

    EXPECT_EQ(dirLight.name, "Sun Light");
    EXPECT_TRUE(dirLight.IsLightNode());
    EXPECT_EQ(dirLight.GetLightType(), LightType::Directional);
    EXPECT_EQ(dirLight.mesh, nullptr); // Lights do not own mesh geometry!

    EXPECT_FLOAT_EQ(dirLight.position.x, 5.0f);
    EXPECT_FLOAT_EQ(dirLight.position.y, 10.0f);
    EXPECT_FLOAT_EQ(dirLight.position.z, 5.0f);

    EXPECT_FLOAT_EQ(dirLight.rotationDegrees.x, -45.0f);
    EXPECT_FLOAT_EQ(dirLight.rotationDegrees.y, 45.0f);
    EXPECT_FLOAT_EQ(dirLight.rotationDegrees.z, 0.0f);

    EXPECT_FLOAT_EQ(dirLight.GetLightIntensity(), 1.5f);
    EXPECT_FLOAT_EQ(dirLight.GetLightColor().r, 1.0f);
    EXPECT_FLOAT_EQ(dirLight.GetLightColor().g, 0.95f);
    EXPECT_FLOAT_EQ(dirLight.GetLightColor().b, 0.85f);

    auto dirComp = dirLight.GetDirectionalLightComponent();
    ASSERT_NE(dirComp, nullptr);
    EXPECT_EQ(dirComp->type, LightType::Directional);
}

TEST(LightNodeTest, PointLightNodeInitializationAndRange) {
    PointLightNode pointLight(
        "Campfire Light",
        glm::vec3(1.0f, 2.0f, 3.0f),
        glm::vec3(1.0f, 0.5f, 0.1f),
        2.5f,
        15.0f
    );

    EXPECT_EQ(pointLight.name, "Campfire Light");
    EXPECT_TRUE(pointLight.IsLightNode());
    EXPECT_EQ(pointLight.GetLightType(), LightType::Point);
    EXPECT_EQ(pointLight.mesh, nullptr);

    EXPECT_FLOAT_EQ(pointLight.position.x, 1.0f);
    EXPECT_FLOAT_EQ(pointLight.position.y, 2.0f);
    EXPECT_FLOAT_EQ(pointLight.position.z, 3.0f);

    EXPECT_FLOAT_EQ(pointLight.GetRange(), 15.0f);
    pointLight.SetRange(20.0f);
    EXPECT_FLOAT_EQ(pointLight.GetRange(), 20.0f);

    auto pointComp = pointLight.GetPointLightComponent();
    ASSERT_NE(pointComp, nullptr);
    EXPECT_EQ(pointComp->type, LightType::Point);
    EXPECT_FLOAT_EQ(pointComp->GetRange(), 20.0f);
}

TEST(LightNodeTest, SpotLightNodeCutoffAnglesAndGeometryParameters) {
    SpotLightNode spotLight(
        "Stage Spotlight",
        glm::vec3(0.0f, 6.0f, 0.0f),
        glm::vec3(-60.0f, 0.0f, 0.0f),
        glm::vec3(0.2f, 0.8f, 1.0f),
        4.0f,
        25.0f,
        18.0f,
        30.0f
    );

    EXPECT_EQ(spotLight.name, "Stage Spotlight");
    EXPECT_TRUE(spotLight.IsLightNode());
    EXPECT_EQ(spotLight.GetLightType(), LightType::Spot);
    EXPECT_EQ(spotLight.mesh, nullptr);

    EXPECT_FLOAT_EQ(spotLight.GetInnerCutoffAngle(), 18.0f);
    EXPECT_FLOAT_EQ(spotLight.GetOuterCutoffAngle(), 30.0f);

    spotLight.SetInnerCutoffAngle(20.0f);
    spotLight.SetOuterCutoffAngle(35.0f);
    EXPECT_FLOAT_EQ(spotLight.GetInnerCutoffAngle(), 20.0f);
    EXPECT_FLOAT_EQ(spotLight.GetOuterCutoffAngle(), 35.0f);

    auto spotComp = spotLight.GetSpotLightComponent();
    ASSERT_NE(spotComp, nullptr);
    EXPECT_EQ(spotComp->type, LightType::Spot);
    EXPECT_FLOAT_EQ(spotComp->innerCutoffAngle, 20.0f);
    EXPECT_FLOAT_EQ(spotComp->outerCutoffAngle, 35.0f);
}

TEST(LightNodeTest, LiskovSubstitutionInSceneHierarchy) {
    auto root = std::make_unique<SceneNode>("Scene Root");
    EXPECT_FALSE(root->IsLightNode());

    SceneNode* dirPtr = root->AddChild(std::make_unique<DirectionalLightNode>("Sun"));
    SceneNode* pointPtr = root->AddChild(std::make_unique<PointLightNode>("Point"));
    SceneNode* spotPtr = root->AddChild(std::make_unique<SpotLightNode>("Spot"));

    ASSERT_EQ(root->GetChildren().size(), 3u);

    EXPECT_TRUE(dirPtr->IsLightNode());
    EXPECT_TRUE(pointPtr->IsLightNode());
    EXPECT_TRUE(spotPtr->IsLightNode());

    EXPECT_EQ(dirPtr->mesh, nullptr);
    EXPECT_EQ(pointPtr->mesh, nullptr);
    EXPECT_EQ(spotPtr->mesh, nullptr);

    // Verify dynamic downcasting works as expected
    auto* typedDir = dynamic_cast<DirectionalLightNode*>(dirPtr);
    ASSERT_NE(typedDir, nullptr);
    EXPECT_EQ(typedDir->GetLightType(), LightType::Directional);
}

TEST(LightNodeTest, LightNodeNeverIngestsCubeFromNodeGraph) {
    auto lightNode = std::make_unique<PointLightNode>("Safe Point Light");
    EXPECT_EQ(lightNode->mesh, nullptr);

    // Trigger NodeGraph evaluation
    VulkanContext* nullContext = nullptr;
    auto graph = lightNode->GetOrCreateNodeGraph(nullContext);
    EXPECT_NE(graph, nullptr);
    lightNode->EvaluateNodeGraph(false);

    // Under SOLID separation, a LightNode must remain mesh-less (never get a cube mesh assigned)
    EXPECT_EQ(lightNode->mesh, nullptr);
}

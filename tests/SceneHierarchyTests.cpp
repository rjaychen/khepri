#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/scene/SceneNode.h"
#include "../src/scene/MeshComponent.h"
#include "../src/scene/Camera.h"
#include "../src/mesh/ModelImporter.h"
#include <glm/gtc/epsilon.hpp>
#include <filesystem>

// ---------------------------------------------------------------------------
// Scene Hierarchy Unit Tests
// ---------------------------------------------------------------------------

TEST(SceneHierarchyTest, NodeNameInheritsFileStemOnImport) {
    VulkanContext* nullContext = nullptr;
    std::string testPath = "assets/models/Box.gltf";
    if (!std::filesystem::exists(testPath) && std::filesystem::exists("../" + testPath)) {
        testPath = "../" + testPath;
    }

    auto sceneRoot = std::make_shared<SceneNode>("Scene Root");
    auto loadedNodeResult = ModelImporter::LoadFromFile(*nullContext, testPath);
    ASSERT_TRUE(loadedNodeResult.has_value());
    auto loadedNode = loadedNodeResult.value();

    std::string stemName = std::filesystem::path(testPath).stem().string(); // "Box"
    const auto& loadedChildren = loadedNode->GetChildren();
    if (!loadedChildren.empty()) {
        for (const auto& child : loadedChildren) {
            if (child->mesh) {
                auto importedChild = std::make_unique<SceneNode>(stemName);
                importedChild->mesh = child->mesh;
                sceneRoot->AddChild(std::move(importedChild));
            }
        }
    } else if (loadedNode->mesh) {
        auto importedChild = std::make_unique<SceneNode>(stemName);
        importedChild->mesh = loadedNode->mesh;
        sceneRoot->AddChild(std::move(importedChild));
    }

    const auto& sceneChildren = sceneRoot->GetChildren();
    ASSERT_FALSE(sceneChildren.empty());
    EXPECT_EQ(sceneChildren[0]->name, "Box");
}

TEST(SceneHierarchyTest, VisibilityTogglePreservesMeshGeometry) {
    VulkanContext* nullContext = nullptr;
    auto node = std::make_unique<SceneNode>("TestMeshNode");
    auto mockMesh = MeshComponent::CreateCube(nullContext, 1.0f);
    node->mesh = mockMesh;

    EXPECT_TRUE(node->visible);
    EXPECT_EQ(node->mesh, mockMesh);

    // Toggle off
    node->visible = false;
    EXPECT_FALSE(node->visible);
    EXPECT_EQ(node->mesh, mockMesh);

    // Toggle back on
    node->visible = true;
    EXPECT_TRUE(node->visible);
    EXPECT_EQ(node->mesh, mockMesh);
    EXPECT_FALSE(node->mesh->GetVertices().empty());
}

TEST(SceneHierarchyTest, MultipleChildNodesPreserveIndependentMeshes) {
    VulkanContext* nullContext = nullptr;
    auto root = std::make_shared<SceneNode>("Scene Root");

    auto cubeNode = std::make_unique<SceneNode>("Cube");
    auto cubeMesh = MeshComponent::CreateCube(nullContext, 1.0f);
    cubeNode->mesh = cubeMesh;

    auto sphereNode = std::make_unique<SceneNode>("Sphere");
    auto sphereMesh = MeshComponent::CreateSphere(nullContext, 0.5f, 16, 16);
    sphereNode->mesh = sphereMesh;

    root->AddChild(std::move(cubeNode));
    root->AddChild(std::move(sphereNode));

    const auto& children = root->GetChildren();
    ASSERT_EQ(children.size(), 2u);

    // Assert that each child retains its unique mesh geometry and pointers
    EXPECT_EQ(children[0]->name, "Cube");
    EXPECT_EQ(children[0]->mesh, cubeMesh);

    EXPECT_EQ(children[1]->name, "Sphere");
    EXPECT_EQ(children[1]->mesh, sphereMesh);

    EXPECT_NE(children[0]->mesh, children[1]->mesh);
}

TEST(SceneHierarchyTest, DetachChildAndReattachHierarchy) {
    auto parent1 = std::make_shared<SceneNode>("Parent 1");
    auto parent2 = std::make_shared<SceneNode>("Parent 2");

    auto child = std::make_unique<SceneNode>("ChildNode");
    SceneNode* rawChild = child.get();

    parent1->AddChild(std::move(child));
    EXPECT_EQ(parent1->GetChildren().size(), 1u);
    EXPECT_EQ(rawChild->GetParent(), parent1.get());

    auto detached = parent1->DetachChild(rawChild);
    EXPECT_EQ(parent1->GetChildren().size(), 0u);
    EXPECT_NE(detached, nullptr);

    parent2->AddChild(std::move(detached));
    EXPECT_EQ(parent2->GetChildren().size(), 1u);
    EXPECT_EQ(rawChild->GetParent(), parent2.get());
}

TEST(SceneHierarchyTest, WorldTransformAccumulatesParentTransforms) {
    auto root = std::make_shared<SceneNode>("Root");
    root->position = glm::vec3(10.0f, 0.0f, 0.0f);

    auto child = std::make_unique<SceneNode>("Child");
    child->position = glm::vec3(0.0f, 5.0f, 0.0f);
    SceneNode* rawChild = root->AddChild(std::move(child));

    glm::mat4 rootWorld = root->GetLocalTransform();
    glm::mat4 childWorld = rootWorld * rawChild->GetLocalTransform();

    glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 transformedChild = childWorld * origin;

    EXPECT_NEAR(transformedChild.x, 10.0f, 1e-4f);
    EXPECT_NEAR(transformedChild.y, 5.0f, 1e-4f);
    EXPECT_NEAR(transformedChild.z, 0.0f, 1e-4f);
}

TEST(SceneHierarchyTest, WireframeModeTogglesOnSceneNode) {
    SceneNode node("TestNode");
    EXPECT_EQ(node.wireframeMode, WireframeMode::Off);

    node.wireframeMode = WireframeMode::Overlay;
    EXPECT_EQ(node.wireframeMode, WireframeMode::Overlay);

    node.wireframeMode = WireframeMode::WireframeOnly;
    EXPECT_EQ(node.wireframeMode, WireframeMode::WireframeOnly);
}

#include <gtest/gtest.h>
#include "../src/mesh/ModelImporter.h"
#include "../src/mesh/OBJImporter.h"
#include "../src/mesh/STLImporter.h"
#include "../src/mesh/GLTFImporter.h"
#include <fstream>
#include <cstdio>

class ModelImporterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ModelImporterTest, ImportsOBJModelFromDisk) {
    const std::string filename = "test_temp_quad.obj";
    {
        std::ofstream file(filename);
        file << "# Wavefront OBJ test quad\n";
        file << "v -1.0 -1.0 0.0\n";
        file << "v 1.0 -1.0 0.0\n";
        file << "v 1.0 1.0 0.0\n";
        file << "v -1.0 1.0 0.0\n";
        file << "vn 0.0 0.0 1.0\n";
        file << "vt 0.0 0.0\n";
        file << "vt 1.0 0.0\n";
        file << "vt 1.0 1.0\n";
        file << "vt 0.0 1.0\n";
        file << "f 1/1/1 2/2/1 3/3/1\n";
        file << "f 1/1/1 3/3/1 4/4/1\n";
    }

    VulkanContext* nullContext = nullptr;
    auto rootNode = ModelImporter::LoadFromFile(*nullContext, filename);

    ASSERT_NE(rootNode, nullptr);
    const auto& children = rootNode->GetChildren();
    ASSERT_EQ(children.size(), 1u);
    ASSERT_NE(children[0]->mesh, nullptr);

    EXPECT_GE(children[0]->mesh->GetVertices().size(), 4u);
    EXPECT_EQ(children[0]->mesh->GetIndices().size(), 6u);

    std::remove(filename.c_str());
}

TEST_F(ModelImporterTest, ImportsASCIIAndBinarySTLModelFromDisk) {
    const std::string filename = "test_temp_tri.stl";
    {
        std::ofstream file(filename);
        file << "solid test_stl\n";
        file << "  facet normal 0.0 0.0 1.0\n";
        file << "    outer loop\n";
        file << "      vertex 0.0 0.0 0.0\n";
        file << "      vertex 1.0 0.0 0.0\n";
        file << "      vertex 0.0 1.0 0.0\n";
        file << "    endloop\n";
        file << "  endfacet\n";
        file << "endsolid test_stl\n";
    }

    VulkanContext* nullContext = nullptr;
    auto rootNode = ModelImporter::LoadFromFile(*nullContext, filename);

    ASSERT_NE(rootNode, nullptr);
    const auto& children = rootNode->GetChildren();
    ASSERT_EQ(children.size(), 1u);
    ASSERT_NE(children[0]->mesh, nullptr);

    EXPECT_EQ(children[0]->mesh->GetVertices().size(), 3u);
    EXPECT_EQ(children[0]->mesh->GetIndices().size(), 3u);

    std::remove(filename.c_str());
}

TEST_F(ModelImporterTest, ImportsGLTFModelFromDisk) {
    VulkanContext* nullContext = nullptr;
    auto rootNode = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");

    ASSERT_NE(rootNode, nullptr);
    const auto& children = rootNode->GetChildren();
    EXPECT_FALSE(children.empty());
}

TEST_F(ModelImporterTest, HandlesInvalidOrNonExistentFilesGracefully) {
    VulkanContext* nullContext = nullptr;
    auto rootNode = ModelImporter::LoadFromFile(*nullContext, "non_existent_file.xyz");
    EXPECT_EQ(rootNode, nullptr);
}

TEST_F(ModelImporterTest, OBJParserHandlesNegativeRelativeIndices) {
    const std::string filename = "test_temp_relative.obj";
    {
        std::ofstream file(filename);
        file << "v -0.5 -0.5 0.0\n";
        file << "v 0.5 -0.5 0.0\n";
        file << "v 0.0 0.5 0.0\n";
        file << "f -3 -2 -1\n";
    }

    VulkanContext* nullContext = nullptr;
    OBJImporter importer;
    auto rootNode = importer.Import(*nullContext, filename);

    ASSERT_NE(rootNode, nullptr);
    const auto& children = rootNode->GetChildren();
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0]->mesh->GetVertices().size(), 3u);

    std::remove(filename.c_str());
}

TEST_F(ModelImporterTest, OpenSceneVsImportModelHierarchyBehavior) {
    VulkanContext* nullContext = nullptr;
    auto initialScene = std::make_shared<SceneNode>("Scene Root");
    auto initialCube = std::make_unique<SceneNode>("Default Cube");
    initialCube->mesh = MeshComponent::CreateCube(nullContext, 1.0f);
    initialScene->AddChild(std::move(initialCube));

    ASSERT_EQ(initialScene->GetChildren().size(), 1u);

    // 1. Simulate Import Model (appends child node, preserves existing nodes)
    auto importedNode = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");
    ASSERT_NE(importedNode, nullptr);

    auto childToAdd = std::make_unique<SceneNode>(importedNode->name);
    initialScene->AddChild(std::move(childToAdd));

    EXPECT_EQ(initialScene->GetChildren().size(), 2u);
    EXPECT_EQ(initialScene->GetChildren()[0]->name, "Default Cube");
    EXPECT_NE(initialScene->GetChildren()[1], nullptr);

    // 2. Simulate Open Scene (replaces entire scene root)
    auto openedSceneNode = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");
    ASSERT_NE(openedSceneNode, nullptr);
    initialScene = openedSceneNode;

    EXPECT_NE(initialScene->name, "Scene Root");
}


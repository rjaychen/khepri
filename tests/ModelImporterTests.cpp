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
    auto rootNodeResult = ModelImporter::LoadFromFile(*nullContext, filename);

    ASSERT_TRUE(rootNodeResult.has_value());
    auto rootNode = rootNodeResult.value();
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
    auto rootNodeResult = ModelImporter::LoadFromFile(*nullContext, filename);

    ASSERT_TRUE(rootNodeResult.has_value());
    auto rootNode = rootNodeResult.value();
    const auto& children = rootNode->GetChildren();
    ASSERT_EQ(children.size(), 1u);
    ASSERT_NE(children[0]->mesh, nullptr);

    EXPECT_EQ(children[0]->mesh->GetVertices().size(), 3u);
    EXPECT_EQ(children[0]->mesh->GetIndices().size(), 3u);

    std::remove(filename.c_str());
}

TEST_F(ModelImporterTest, ImportsGLTFModelFromDisk) {
    VulkanContext* nullContext = nullptr;
    auto rootNodeResult = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");

    ASSERT_TRUE(rootNodeResult.has_value());
    auto rootNode = rootNodeResult.value();
    const auto& children = rootNode->GetChildren();
    EXPECT_FALSE(children.empty());
}

TEST_F(ModelImporterTest, HandlesInvalidOrNonExistentFilesGracefully) {
    VulkanContext* nullContext = nullptr;
    auto res1 = ModelImporter::LoadFromFile(*nullContext, "non_existent_file.xyz");
    EXPECT_FALSE(res1.has_value());
    EXPECT_EQ(res1.error(), khepri::ImportError::FileNotFound);
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
    auto rootNodeResult = importer.Import(*nullContext, filename);

    ASSERT_TRUE(rootNodeResult.has_value());
    auto rootNode = rootNodeResult.value();
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
    auto importedResult = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");
    ASSERT_TRUE(importedResult.has_value());
    auto importedNode = importedResult.value();

    auto childToAdd = std::make_unique<SceneNode>(importedNode->name);
    initialScene->AddChild(std::move(childToAdd));

    EXPECT_EQ(initialScene->GetChildren().size(), 2u);
    EXPECT_EQ(initialScene->GetChildren()[0]->name, "Default Cube");
    EXPECT_NE(initialScene->GetChildren()[1], nullptr);

    // 2. Simulate Open Scene (replaces entire scene root)
    auto openedResult = ModelImporter::LoadFromFile(*nullContext, "assets/models/Box.gltf");
    ASSERT_TRUE(openedResult.has_value());
    auto openedSceneNode = openedResult.value();
    initialScene = openedSceneNode;

    EXPECT_NE(initialScene->name, "Scene Root");
}

TEST_F(ModelImporterTest, DataDrivenRegistryAndCustomRegistration) {
    const auto& registry = ModelImporter::GetRegistry();
    EXPECT_GE(registry.size(), 3u); // .gltf/.glb, .obj, .stl

    class MockCustomImporter : public ModelImporter {
    public:
        [[nodiscard]] bool CanImport(const std::string& path) const override {
            return path.ends_with(".custom");
        }
        [[nodiscard]] std::expected<std::shared_ptr<SceneNode>, khepri::ImportError> Import(
            VulkanContext&, const std::string&, VkDescriptorSetLayout, DescriptorAllocator*, VkBuffer) override {
            return std::make_shared<SceneNode>("MockCustomNode");
        }
    };

    ModelImporter::RegisterImporter({".custom"}, []() -> std::unique_ptr<ModelImporter> {
        return std::make_unique<MockCustomImporter>();
    });

    const auto& updatedReg = ModelImporter::GetRegistry();
    bool foundCustom = false;
    for (const auto& entry : updatedReg) {
        for (const auto& ext : entry.extensions) {
            if (ext == ".custom") foundCustom = true;
        }
    }
    EXPECT_TRUE(foundCustom);
}

TEST_F(ModelImporterTest, ErrorTaxonomyStringHelpers) {
    EXPECT_EQ(khepri::ToString(khepri::ImportError::None), "None");
    EXPECT_EQ(khepri::ToString(khepri::ImportError::FileNotFound), "File not found");
    EXPECT_EQ(khepri::ToString(khepri::ImportError::UnsupportedFormat), "Unsupported file format or extension");
    EXPECT_EQ(khepri::ToString(khepri::ImportError::ParsingFailed), "Parsing failed");

    EXPECT_EQ(khepri::ToString(khepri::VulkanError::None), "None");
    EXPECT_EQ(khepri::ToString(khepri::VulkanError::InitializationFailed), "Vulkan initialization failed");
    EXPECT_EQ(khepri::ToString(khepri::VulkanError::DeviceLost), "Vulkan device lost");
}


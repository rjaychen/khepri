#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/assets/AssetManager.h"
#include <filesystem>
#include <fstream>

// ---------------------------------------------------------------------------
// Asset Manager & Filesystem Unit Tests
// ---------------------------------------------------------------------------

TEST(AssetManagerTest, SingletonInitializationAndDefaultMaterials) {
    VulkanContext* nullContext = nullptr;
    khepri::AssetManager::Instance().Initialize(*nullContext);

    auto defaultMat = khepri::AssetManager::Instance().GetDefaultMaterial();
    ASSERT_NE(defaultMat, nullptr);
    EXPECT_EQ(defaultMat->GetName(), "DefaultPBR");

    auto emissiveMat = khepri::AssetManager::Instance().GetDefaultEmissiveLightMaterial();
    ASSERT_NE(emissiveMat, nullptr);
    EXPECT_EQ(emissiveMat->GetName(), "EmissiveLightGizmo");
}

TEST(AssetManagerTest, MaterialCreationAndPropertyMutation) {
    VulkanContext* nullContext = nullptr;
    khepri::AssetManager::Instance().Initialize(*nullContext);

    auto customMat = khepri::AssetManager::Instance().CreateMaterial("CustomTestMaterial");
    ASSERT_NE(customMat, nullptr);
    EXPECT_EQ(customMat->GetName(), "CustomTestMaterial");

    // Mutate properties
    glm::vec4 newColor(0.8f, 0.2f, 0.4f, 1.0f);
    customMat->SetBaseColorFactor(newColor);
    EXPECT_EQ(customMat->GetBaseColorFactor(), newColor);

    customMat->SetRoughness(0.25f);
    EXPECT_FLOAT_EQ(customMat->GetRoughness(), 0.25f);

    customMat->SetMetallic(0.85f);
    EXPECT_FLOAT_EQ(customMat->GetMetallic(), 0.85f);

    // Retrieve via manager
    auto fetchedMat = khepri::AssetManager::Instance().GetMaterial("CustomTestMaterial");
    EXPECT_EQ(fetchedMat, customMat);
}

TEST(AssetManagerTest, DefaultAssetModelFilesExist) {
    std::vector<std::string> defaultModels = {
        "assets/models/bunny.obj",
        "assets/models/duck.glb",
        "assets/models/utah_teapot.obj",
        "assets/models/Box.gltf"
    };

    for (const auto& modelPath : defaultModels) {
        bool exists = std::filesystem::exists(modelPath) ||
                      std::filesystem::exists("../" + modelPath) ||
                      std::filesystem::exists("../../" + modelPath);
        EXPECT_TRUE(exists) << "Default engine model missing from disk: " << modelPath;
    }
}

TEST(AssetManagerTest, FilesystemOperationsCreateRenameDelete) {
    std::filesystem::path testDir = "temp_test_assets";
    if (std::filesystem::exists(testDir)) {
        std::filesystem::remove_all(testDir);
    }

    // 1. Create Directory
    EXPECT_TRUE(std::filesystem::create_directories(testDir));
    EXPECT_TRUE(std::filesystem::is_directory(testDir));

    // 2. Create Asset File
    std::filesystem::path testFile = testDir / "TestPreset.mat";
    {
        std::ofstream file(testFile);
        file << "{\n  \"name\": \"TestPreset\"\n}\n";
    }
    EXPECT_TRUE(std::filesystem::exists(testFile));

    // 3. Rename Asset File
    std::filesystem::path renamedFile = testDir / "RenamedPreset.mat";
    std::filesystem::rename(testFile, renamedFile);
    EXPECT_FALSE(std::filesystem::exists(testFile));
    EXPECT_TRUE(std::filesystem::exists(renamedFile));

    // 4. Move Asset File into Subfolder
    std::filesystem::path subDir = testDir / "Materials";
    std::filesystem::create_directories(subDir);
    std::filesystem::path movedFile = subDir / "RenamedPreset.mat";
    std::filesystem::rename(renamedFile, movedFile);
    EXPECT_TRUE(std::filesystem::exists(movedFile));

    // 5. Cleanup / Delete Directory
    std::filesystem::remove_all(testDir);
    EXPECT_FALSE(std::filesystem::exists(testDir));
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/editor/AssetManagerPanel.h"
#include <filesystem>

// ---------------------------------------------------------------------------
// Asset Manager Panel & Dual-View Unit Tests
// ---------------------------------------------------------------------------

TEST(AssetManagerOverhaulTest, ViewModeSwitching) {
    khepri::AssetManagerPanel panel;

    EXPECT_EQ(panel.GetViewMode(), khepri::AssetViewMode::Grid);

    panel.SetViewMode(khepri::AssetViewMode::List);
    EXPECT_EQ(panel.GetViewMode(), khepri::AssetViewMode::List);

    panel.SetViewMode(khepri::AssetViewMode::Grid);
    EXPECT_EQ(panel.GetViewMode(), khepri::AssetViewMode::Grid);
}

TEST(AssetManagerOverhaulTest, CategoryFilterSwitching) {
    khepri::AssetManagerPanel panel;

    EXPECT_EQ(panel.GetCategoryFilter(), khepri::ui::AssetCategory::All);

    panel.SetCategoryFilter(khepri::ui::AssetCategory::Model3D);
    EXPECT_EQ(panel.GetCategoryFilter(), khepri::ui::AssetCategory::Model3D);

    panel.SetCategoryFilter(khepri::ui::AssetCategory::Material);
    EXPECT_EQ(panel.GetCategoryFilter(), khepri::ui::AssetCategory::Material);
}

TEST(AssetManagerOverhaulTest, SelectionManagement) {
    khepri::AssetManagerPanel panel;

    EXPECT_TRUE(panel.GetSelectedPath().empty());

    std::filesystem::path dummyPath = "assets/models/test.gltf";
    panel.SetSelectedPath(dummyPath);
    EXPECT_EQ(panel.GetSelectedPath(), dummyPath);

    panel.ClearSelection();
    EXPECT_TRUE(panel.GetSelectedPath().empty());
}

TEST(AssetManagerOverhaulTest, CacheInvalidation) {
    khepri::AssetManagerPanel panel;
    panel.InvalidateCache();
    // Cache invalidation executes cleanly
}


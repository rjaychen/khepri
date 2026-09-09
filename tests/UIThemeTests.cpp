#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/editor/Theme.h"
#include "../src/editor/UIWidgets.h"
#include "../src/editor/ThumbnailCache.h"

// ---------------------------------------------------------------------------
// UI Theme, Typography & Design System Unit Tests
// ---------------------------------------------------------------------------

class UIThemeTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        khepri::ui::Theme::ResetState();
    }
    void TearDown() override {
        khepri::ui::Theme::ResetState();
    }
};

TEST_F(UIThemeTestFixture, ThemeColorTokensMatchOryzoPalette) {
    EXPECT_FLOAT_EQ(khepri::ui::Theme::COLOR_BG_DARK.w, 1.0f);
    EXPECT_LT(khepri::ui::Theme::COLOR_BG_DARK.x, 0.1f);
    EXPECT_GT(khepri::ui::Theme::COLOR_ACCENT_PRIMARY.z, 0.9f);
    EXPECT_GT(khepri::ui::Theme::COLOR_TEXT_PRIMARY.x, 0.9f);
    EXPECT_LT(khepri::ui::Theme::COLOR_TEXT_MUTED.x, 0.4f);
}

TEST_F(UIThemeTestFixture, ApplyThemeSetsImGuiStyleAndScaling) {
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ASSERT_NE(ctx, nullptr);

    // Apply baseline scale 1.0
    khepri::ui::Theme::ApplyTheme(1.0f);
    ImGuiStyle& style = ImGui::GetStyle();

    EXPECT_FLOAT_EQ(style.WindowRounding, 8.0f);
    EXPECT_FLOAT_EQ(style.ChildRounding, 6.0f);
    EXPECT_FLOAT_EQ(style.FrameRounding, 6.0f);
    EXPECT_FLOAT_EQ(style.WindowBorderSize, 1.0f);

    // Apply scaled theme (1.5x)
    khepri::ui::Theme::ApplyTheme(1.5f);
    EXPECT_FLOAT_EQ(style.WindowRounding, 12.0f);
    EXPECT_FLOAT_EQ(style.ChildRounding, 9.0f);
    EXPECT_FLOAT_EQ(style.FrameRounding, 9.0f);

    ImGui::DestroyContext(ctx);
}

TEST_F(UIThemeTestFixture, ScaleCalculationHierarchy) {
    khepri::ui::Theme::SetContentScale(1.25f);
    khepri::ui::Theme::SetUserScale(1.5f);

    EXPECT_FLOAT_EQ(khepri::ui::Theme::GetContentScale(), 1.25f);
    EXPECT_FLOAT_EQ(khepri::ui::Theme::GetUserScale(), 1.5f);
    EXPECT_FLOAT_EQ(khepri::ui::Theme::GetTotalScale(), 1.25f * 1.5f);
}

TEST(UIThemeTest, ThumbnailCacheCategoryClassification) {
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("model.gltf"), khepri::ui::AssetCategory::Model3D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("character.glb"), khepri::ui::AssetCategory::Model3D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("mesh.obj"), khepri::ui::AssetCategory::Model3D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("diffuse.png"), khepri::ui::AssetCategory::Texture2D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("normal.jpg"), khepri::ui::AssetCategory::Texture2D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("roughness.tga"), khepri::ui::AssetCategory::Texture2D);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("gold.mat"), khepri::ui::AssetCategory::Material);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("mesh.vert"), khepri::ui::AssetCategory::Shader);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("mesh.frag"), khepri::ui::AssetCategory::Shader);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("deform.comp"), khepri::ui::AssetCategory::Shader);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("level1.khepri"), khepri::ui::AssetCategory::Scene);
    EXPECT_EQ(khepri::ui::ThumbnailCache::GetCategoryFromPath("readme.txt"), khepri::ui::AssetCategory::Document);
}

TEST(UIThemeTest, ThumbnailCacheLRUEviction) {
    khepri::ui::ThumbnailCache cache(nullptr);

    // Create 10 dummy thumbnail entries
    for (int i = 0; i < 10; ++i) {
        cache.GetOrCreateThumbnail("test_file_" + std::to_string(i) + ".obj");
    }

    // Evict down to capacity 4
    cache.EvictLRU(4);

    // Clear everything
    cache.Clear();
}

TEST(UIThemeTest, ThumbnailCacheVectorIconAssignment) {
    khepri::ui::ThumbnailCache cache(nullptr);

    const auto& modelThumb = cache.GetOrCreateThumbnail("spaceship.gltf");
    EXPECT_EQ(modelThumb.vectorIcon, khepri::ui::VectorIconType::Mesh);
    EXPECT_EQ(modelThumb.category, khepri::ui::AssetCategory::Model3D);

    const auto& texThumb = cache.GetOrCreateThumbnail("albedo.png");
    EXPECT_EQ(texThumb.vectorIcon, khepri::ui::VectorIconType::Texture);

    const auto& matThumb = cache.GetOrCreateThumbnail("metal.mat");
    EXPECT_EQ(matThumb.vectorIcon, khepri::ui::VectorIconType::Material);

    const auto& shaderThumb = cache.GetOrCreateThumbnail("pbr.frag");
    EXPECT_EQ(shaderThumb.vectorIcon, khepri::ui::VectorIconType::Shader);

    const auto& sceneThumb = cache.GetOrCreateThumbnail("level01.khepri");
    EXPECT_EQ(sceneThumb.vectorIcon, khepri::ui::VectorIconType::Scene);

    const auto& docThumb = cache.GetOrCreateThumbnail("notes.txt");
    EXPECT_EQ(docThumb.vectorIcon, khepri::ui::VectorIconType::File);
}

TEST(UIThemeTest, VectorIconsDrawRoutinesExecution) {
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ASSERT_NE(ctx, nullptr);

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(800.0f, 600.0f);

    ImGui::NewFrame();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ASSERT_NE(drawList, nullptr);

    ImVec2 center(50.0f, 50.0f);
    ImU32 col = IM_COL32(255, 255, 255, 255);

    // Test drawing all icon types to ensure no vertex/buffer overflow or crash
    khepri::ui::VectorIcons::DrawMesh(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawSun(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawPointLight(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawSpotLight(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawFolder(drawList, center, 24.0f, col, false);
    khepri::ui::VectorIcons::DrawFolder(drawList, center, 24.0f, col, true);
    khepri::ui::VectorIcons::DrawFile(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawEye(drawList, center, 24.0f, col, true);
    khepri::ui::VectorIcons::DrawEye(drawList, center, 24.0f, col, false);
    khepri::ui::VectorIcons::DrawLock(drawList, center, 24.0f, col, true);
    khepri::ui::VectorIcons::DrawLock(drawList, center, 24.0f, col, false);
    khepri::ui::VectorIcons::DrawCamera(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawMaterial(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawTexture(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawShader(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawScene(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawSearch(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawGrid(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawList(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawPlus(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawTrash(drawList, center, 24.0f, col);
    khepri::ui::VectorIcons::DrawNode(drawList, center, 24.0f, col);

    EXPECT_GT(drawList->VtxBuffer.Size, 0);
    EXPECT_GT(drawList->IdxBuffer.Size, 0);

    ImGui::Render();
    ImGui::DestroyContext(ctx);
}

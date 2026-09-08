#pragma once

#include "../assets/AssetManager.h"
#include "ThumbnailCache.h"
#include "Theme.h"
#include "UIWidgets.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace khepri {

enum class AssetViewMode {
    Grid,
    List
};

class AssetManagerPanel {
public:
    AssetManagerPanel(ui::ThumbnailCache* thumbnailCache = nullptr);
    ~AssetManagerPanel() = default;

    void SetThumbnailCache(ui::ThumbnailCache* cache) { m_thumbnailCache = cache; }

    void RenderUI(bool* p_open = nullptr);

    // Callbacks
    void SetOpenModelCallback(std::function<void(const std::string&)> cb) { m_onOpenModel = std::move(cb); }
    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }

    const std::filesystem::path& GetSelectedPath() const { return m_selectedPath; }
    void ClearSelection() { m_selectedPath.clear(); }
    void SetSelectedPath(const std::filesystem::path& path) { m_selectedPath = path; }

    void NavigateTo(const std::filesystem::path& newPath);

    // View Mode API
    AssetViewMode GetViewMode() const { return m_viewMode; }
    void SetViewMode(AssetViewMode mode) { m_viewMode = mode; }

    // Category Filter API
    ui::AssetCategory GetCategoryFilter() const { return m_categoryFilter; }
    void SetCategoryFilter(ui::AssetCategory category) { m_categoryFilter = category; }

public:
    struct CachedAssetEntry {
        std::filesystem::path path;
        std::string filename;
        bool isDirectory = false;
        uintmax_t fileSize = 0;
        std::string formattedSize;
        std::string formattedTime;
        ui::AssetCategory category = ui::AssetCategory::Unknown;
        std::time_t lastWriteTime = 0;
    };

    void InvalidateCache() { m_cacheDirty = true; }

private:
    void RenderNavigationBar();
    void RenderCategoryFilterBar();
    void RenderFolderTree(const std::filesystem::path& dirPath);
    void RenderFileGrid(const std::vector<CachedAssetEntry>& entries);
    void RenderFileList(const std::vector<CachedAssetEntry>& entries);
    void RenderContextMenu();

    void RefreshCache();
    void UpdateFilteredEntries();
    const std::vector<CachedAssetEntry>& GetFilteredEntries();

    std::vector<CachedAssetEntry> m_cachedEntries;
    std::vector<CachedAssetEntry> m_filteredEntries;
    bool m_cacheDirty = true;
    std::chrono::steady_clock::time_point m_lastCacheRefresh;

    std::filesystem::path m_rootDirectory;
    std::filesystem::path m_currentDirectory;
    std::filesystem::path m_selectedPath;

    ui::ThumbnailCache* m_thumbnailCache{nullptr};
    std::unique_ptr<ui::ThumbnailCache> m_ownedThumbnailCache;

    // View Mode (Grid vs List)
    AssetViewMode m_viewMode{AssetViewMode::Grid};
    float m_gridThumbnailSize{84.0f};

    // Category Filter
    ui::AssetCategory m_categoryFilter{ui::AssetCategory::All};

    // Navigation History
    std::vector<std::filesystem::path> m_history;
    int m_historyIndex{-1};

    // Filter & Search
    char m_searchFilter[128] = "";

    // Dialog modal state
    bool m_showCreateFolderModal{false};
    bool m_showCreateMaterialModal{false};
    bool m_showCreateFileModal{false};
    bool m_showRenameModal{false};
    bool m_showDeleteModal{false};

    char m_nameBuffer[256] = "";
    std::filesystem::path m_actionTargetPath;

    // Callbacks to EditorApp
    std::function<void(const std::string&)> m_onOpenModel;
    std::function<void(const std::string&)> m_onImportModel;
};

} // namespace khepri

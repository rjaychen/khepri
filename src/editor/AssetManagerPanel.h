#pragma once

#include "../assets/AssetManager.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace khepri {

class AssetManagerPanel {
public:
    AssetManagerPanel();
    ~AssetManagerPanel() = default;

    void RenderUI(bool* p_open = nullptr);

    // Callbacks
    void SetOpenModelCallback(std::function<void(const std::string&)> cb) { m_onOpenModel = std::move(cb); }
    void SetImportModelCallback(std::function<void(const std::string&)> cb) { m_onImportModel = std::move(cb); }

    const std::filesystem::path& GetSelectedPath() const { return m_selectedPath; }
    void ClearSelection() { m_selectedPath.clear(); }
    void SetSelectedPath(const std::filesystem::path& path) { m_selectedPath = path; }

private:
    void RenderNavigationBar();
    void RenderFolderTree(const std::filesystem::path& dirPath);
    void RenderFileGrid();
    void RenderContextMenu();
    void NavigateTo(const std::filesystem::path& newPath);
    
    std::filesystem::path m_rootDirectory;
    std::filesystem::path m_currentDirectory;
    std::filesystem::path m_selectedPath;

    // Navigation History
    std::vector<std::filesystem::path> m_history;
    int m_historyIndex = -1;

    // Filter & Search
    char m_searchFilter[128] = "";

    // Dialog modal state
    bool m_showCreateFolderModal = false;
    bool m_showCreateMaterialModal = false;
    bool m_showCreateFileModal = false;
    bool m_showRenameModal = false;
    bool m_showDeleteModal = false;

    char m_nameBuffer[256] = "";
    std::filesystem::path m_actionTargetPath;

    // Callbacks to EditorApp
    std::function<void(const std::string&)> m_onOpenModel;
    std::function<void(const std::string&)> m_onImportModel;
};

} // namespace khepri


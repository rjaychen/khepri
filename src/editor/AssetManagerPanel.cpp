#include "AssetManagerPanel.h"
#include "../core/Logger.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <algorithm>

namespace khepri {

AssetManagerPanel::AssetManagerPanel() {
    m_rootDirectory = std::filesystem::absolute("assets");
    if (!std::filesystem::exists(m_rootDirectory)) {
        std::filesystem::create_directories(m_rootDirectory);
    }
    
    // Ensure assets/models exists
    std::filesystem::path modelsDir = m_rootDirectory / "models";
    if (!std::filesystem::exists(modelsDir)) {
        std::filesystem::create_directories(modelsDir);
    }

    m_currentDirectory = m_rootDirectory;
    m_history.push_back(m_currentDirectory);
    m_historyIndex = 0;
}

void AssetManagerPanel::NavigateTo(const std::filesystem::path& newPath) {
    if (!std::filesystem::exists(newPath) || !std::filesystem::is_directory(newPath)) {
        return;
    }

    // Truncate forward history if we are navigating somewhere new from the middle of history
    if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_history.size()) - 1) {
        m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    }

    m_history.push_back(newPath);
    m_historyIndex = static_cast<int>(m_history.size()) - 1;
    m_currentDirectory = newPath;
}

void AssetManagerPanel::RenderNavigationBar() {
    // History Back Button
    bool canGoBack = (m_historyIndex > 0);
    if (!canGoBack) ImGui::BeginDisabled();
    if (ImGui::Button("<")) {
        if (canGoBack) {
            m_historyIndex--;
            m_currentDirectory = m_history[m_historyIndex];
        }
    }
    if (!canGoBack) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

    ImGui::SameLine();

    // History Forward Button
    bool canGoForward = (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_history.size()) - 1);
    if (!canGoForward) ImGui::BeginDisabled();
    if (ImGui::Button(">")) {
        if (canGoForward) {
            m_historyIndex++;
            m_currentDirectory = m_history[m_historyIndex];
        }
    }
    if (!canGoForward) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");

    ImGui::SameLine();

    // Up Directory Button
    bool canGoUp = (m_currentDirectory != m_rootDirectory && m_currentDirectory.has_parent_path());
    if (!canGoUp) ImGui::BeginDisabled();
    if (ImGui::Button("^ Up")) {
        if (canGoUp) {
            NavigateTo(m_currentDirectory.parent_path());
        }
    }
    if (!canGoUp) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Parent Directory");

    ImGui::SameLine();

    // Breadcrumb Trail / Path
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Location:");
    ImGui::SameLine();
    
    std::filesystem::path relPath = std::filesystem::relative(m_currentDirectory, m_rootDirectory.parent_path());
    std::string pathStr = relPath.string();
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
    ImGui::TextUnformatted(pathStr.c_str());

    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##SearchFilter", "Search...", m_searchFilter, sizeof(m_searchFilter));

    ImGui::Separator();
}

void AssetManagerPanel::RenderFolderTree(const std::filesystem::path& dirPath) {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) return;

    std::string folderName = dirPath.filename().string();
    if (dirPath == m_rootDirectory) {
        folderName = "assets (Root)";
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (dirPath == m_currentDirectory) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool hasSubdirs = false;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_directory()) {
                hasSubdirs = true;
                break;
            }
        }
    } catch (...) {}

    if (!hasSubdirs) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool opened = ImGui::TreeNodeEx(dirPath.string().c_str(), flags, "%s %s", "📁", folderName.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        NavigateTo(dirPath);
    }

    // Drop target for moving assets into this folder tree node
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH_PAYLOAD")) {
            std::filesystem::path sourcePath(static_cast<const char*>(payload->Data));
            if (std::filesystem::exists(sourcePath) && sourcePath.parent_path() != dirPath) {
                std::filesystem::path targetPath = dirPath / sourcePath.filename();
                try {
                    std::filesystem::rename(sourcePath, targetPath);
                    LOG_INFO("Moved asset to: " + targetPath.string());
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to move asset: " + std::string(e.what()));
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (opened) {
        if (hasSubdirs) {
            try {
                for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                    if (entry.is_directory()) {
                        RenderFolderTree(entry.path());
                    }
                }
            } catch (...) {}
        }
        ImGui::TreePop();
    }
}

void AssetManagerPanel::RenderFileGrid() {
    if (!std::filesystem::exists(m_currentDirectory)) return;

    std::vector<std::filesystem::directory_entry> entries;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory)) {
            if (strlen(m_searchFilter) > 0) {
                std::string filename = entry.path().filename().string();
                std::string filterStr(m_searchFilter);
                auto it = std::search(filename.begin(), filename.end(), filterStr.begin(), filterStr.end(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                if (it == filename.end()) continue;
            }
            entries.push_back(entry);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error reading directory: " + std::string(e.what()));
    }

    // Sort: directories first, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if (a.is_directory() != b.is_directory()) {
            return a.is_directory();
        }
        return a.path().filename() < b.path().filename();
    });

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& itemPath = entry.path();
        std::string filename = itemPath.filename().string();
        bool isDir = entry.is_directory();
        std::string ext = itemPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Icon indicator & type string
        const char* icon = "📄";
        bool isModel = false;
        if (isDir) {
            icon = "📁";
        } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".stl") {
            icon = "📦";
            isModel = true;
        } else if (ext == ".mat") {
            icon = "🎨";
        } else if (ext == ".scene" || ext == ".khepri") {
            icon = "🎬";
        } else if (ext == ".png" || ext == ".jpg" || ext == ".tga") {
            icon = "🖼️";
        }

        bool isSelected = (itemPath == m_selectedPath);

        ImGui::PushID(static_cast<int>(i));

        std::string label = std::string(icon) + "  " + filename;
        if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 22))) {
            m_selectedPath = itemPath;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (isDir) {
                    NavigateTo(itemPath);
                } else if (isModel && m_onOpenModel) {
                    m_onOpenModel(itemPath.string());
                }
            }
        }

        // Drag and Drop Source (Drag item from list)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            std::string pathStr = itemPath.string();
            ImGui::SetDragDropPayload("ASSET_PATH_PAYLOAD", pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("%s %s", icon, filename.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop Target (if item is a folder, accept dragging items into it)
        if (isDir && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH_PAYLOAD")) {
                std::filesystem::path sourcePath(static_cast<const char*>(payload->Data));
                if (std::filesystem::exists(sourcePath) && sourcePath != itemPath) {
                    std::filesystem::path targetPath = itemPath / sourcePath.filename();
                    try {
                        std::filesystem::rename(sourcePath, targetPath);
                        LOG_INFO("Moved asset to: " + targetPath.string());
                    } catch (const std::exception& e) {
                        LOG_ERROR("Failed to move asset: " + std::string(e.what()));
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Context menu per item
        if (ImGui::BeginPopupContextItem("ItemContext")) {
            m_actionTargetPath = itemPath;
            m_selectedPath = itemPath;
            
            if (isModel && ImGui::MenuItem("Open Scene with Model")) {
                if (m_onOpenModel) m_onOpenModel(itemPath.string());
            }
            if (isModel && ImGui::MenuItem("Import Model into Scene")) {
                if (m_onImportModel) m_onImportModel(itemPath.string());
            }
            if (ImGui::MenuItem("Rename")) {
                strncpy(m_nameBuffer, filename.c_str(), sizeof(m_nameBuffer));
                m_nameBuffer[sizeof(m_nameBuffer) - 1] = '\0';
                m_showRenameModal = true;
            }
            if (ImGui::MenuItem("Delete")) {
                m_showDeleteModal = true;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::PopStyleVar();

    // Context menu for empty list space
    if (ImGui::BeginPopupContextWindow("GridBackgroundContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("+ New Folder")) {
            m_nameBuffer[0] = '\0';
            m_showCreateFolderModal = true;
        }
        if (ImGui::MenuItem("+ New Material (.mat)")) {
            strncpy(m_nameBuffer, "NewMaterial.mat", sizeof(m_nameBuffer));
            m_showCreateMaterialModal = true;
        }
        if (ImGui::MenuItem("+ New Text File (.txt)")) {
            strncpy(m_nameBuffer, "NewFile.txt", sizeof(m_nameBuffer));
            m_showCreateFileModal = true;
        }
        ImGui::EndPopup();
    }
}

void AssetManagerPanel::RenderContextMenu() {
    // 1. Create Folder Modal
    if (m_showCreateFolderModal) {
        ImGui::OpenPopup("Create Folder##Modal");
        m_showCreateFolderModal = false;
    }
    if (ImGui::BeginPopupModal("Create Folder##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        ImGui::InputText("##FolderName", m_nameBuffer, sizeof(m_nameBuffer));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (strlen(m_nameBuffer) > 0) {
                std::filesystem::path newDir = m_currentDirectory / m_nameBuffer;
                try {
                    std::filesystem::create_directory(newDir);
                    LOG_INFO("Created folder: " + newDir.string());
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to create folder: " + std::string(e.what()));
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. Create Material Modal
    if (m_showCreateMaterialModal) {
        ImGui::OpenPopup("Create Material##Modal");
        m_showCreateMaterialModal = false;
    }
    if (ImGui::BeginPopupModal("Create Material##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter material filename (.mat):");
        ImGui::InputText("##MatName", m_nameBuffer, sizeof(m_nameBuffer));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (strlen(m_nameBuffer) > 0) {
                std::string matName = m_nameBuffer;
                if (matName.find(".mat") == std::string::npos) matName += ".mat";
                std::filesystem::path newMatFile = m_currentDirectory / matName;
                
                std::ofstream file(newMatFile);
                if (file.is_open()) {
                    file << "{\n  \"name\": \"" << newMatFile.stem().string() << "\",\n";
                    file << "  \"baseColor\": [1.0, 1.0, 1.0, 1.0],\n";
                    file << "  \"roughness\": 0.5,\n  \"metallic\": 0.0\n}\n";
                    file.close();
                    AssetManager::Instance().CreateMaterial(newMatFile.stem().string());
                    LOG_INFO("Created material file: " + newMatFile.string());
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 3. Create File Modal
    if (m_showCreateFileModal) {
        ImGui::OpenPopup("Create File##Modal");
        m_showCreateFileModal = false;
    }
    if (ImGui::BeginPopupModal("Create File##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file name:");
        ImGui::InputText("##FileName", m_nameBuffer, sizeof(m_nameBuffer));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (strlen(m_nameBuffer) > 0) {
                std::filesystem::path newFile = m_currentDirectory / m_nameBuffer;
                std::ofstream file(newFile);
                if (file.is_open()) {
                    file << "// New asset file created in Khepri Engine\n";
                    file.close();
                    LOG_INFO("Created file: " + newFile.string());
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 4. Rename Modal
    if (m_showRenameModal) {
        ImGui::OpenPopup("Rename Asset##Modal");
        m_showRenameModal = false;
    }
    if (ImGui::BeginPopupModal("Rename Asset##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename '%s' to:", m_actionTargetPath.filename().string().c_str());
        ImGui::InputText("##NewName", m_nameBuffer, sizeof(m_nameBuffer));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            if (strlen(m_nameBuffer) > 0) {
                std::filesystem::path newPath = m_actionTargetPath.parent_path() / m_nameBuffer;
                try {
                    std::filesystem::rename(m_actionTargetPath, newPath);
                    if (m_selectedPath == m_actionTargetPath) m_selectedPath = newPath;
                    LOG_INFO("Renamed asset to: " + newPath.string());
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to rename asset: " + std::string(e.what()));
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 5. Delete Modal
    if (m_showDeleteModal) {
        ImGui::OpenPopup("Delete Asset##Modal");
        m_showDeleteModal = false;
    }
    if (ImGui::BeginPopupModal("Delete Asset##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Are you sure you want to delete '%s'?",
            m_actionTargetPath.filename().string().c_str());
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            try {
                std::filesystem::remove_all(m_actionTargetPath);
                if (m_selectedPath == m_actionTargetPath) m_selectedPath.clear();
                LOG_INFO("Deleted asset: " + m_actionTargetPath.string());
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to delete asset: " + std::string(e.what()));
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetManagerPanel::RenderUI(bool* p_open) {
    if (p_open && !*p_open) return;

    if (!ImGui::Begin("Asset Manager", p_open)) {
        ImGui::End();
        return;
    }

    RenderNavigationBar();

    ImGui::Columns(2, "AssetManagerSplit", true);
    ImGui::SetColumnWidth(0, 220.0f);

    // Left Column: Directory Tree
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Folders");
    ImGui::Separator();
    ImGui::BeginChild("FolderTreeScroll", ImVec2(0, 0), false);
    RenderFolderTree(m_rootDirectory);
    ImGui::EndChild();

    ImGui::NextColumn();

    // Right Column: File Grid
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Contents");
    ImGui::Separator();
    ImGui::BeginChild("FileGridScroll", ImVec2(0, 0), false);
    RenderFileGrid();
    ImGui::EndChild();

    ImGui::Columns(1);

    RenderContextMenu();

    ImGui::End();
}

} // namespace khepri


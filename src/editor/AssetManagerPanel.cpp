#include "AssetManagerPanel.h"
#include "Icons.h"
#include "VectorIcons.h"
#include "Theme.h"
#include "UIWidgets.h"
#include "../core/Logger.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace khepri {

using namespace khepri::ui;

static std::filesystem::path FindAssetsRootDirectory() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "assets",
        std::filesystem::current_path() / "../assets",
        std::filesystem::current_path() / "../../assets",
        std::filesystem::current_path() / "../../../assets"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) {
            if (std::filesystem::exists(c / "models") || std::filesystem::exists(c / "materials") || std::filesystem::exists(c / "fonts")) {
                try {
                    return std::filesystem::canonical(c);
                } catch (...) {
                    return std::filesystem::absolute(c);
                }
            }
        }
    }
    return std::filesystem::absolute("assets");
}

AssetManagerPanel::AssetManagerPanel(ui::ThumbnailCache* thumbnailCache)
    : m_thumbnailCache(thumbnailCache) {
    if (!m_thumbnailCache) {
        m_ownedThumbnailCache = std::make_unique<ui::ThumbnailCache>();
        m_thumbnailCache = m_ownedThumbnailCache.get();
    }

    m_rootDirectory = FindAssetsRootDirectory();
    if (!std::filesystem::exists(m_rootDirectory)) {
        std::filesystem::create_directories(m_rootDirectory);
    }

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

    if (m_historyIndex >= 0 && m_historyIndex < static_cast<int>(m_history.size()) - 1) {
        m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    }

    m_history.push_back(newPath);
    m_historyIndex = static_cast<int>(m_history.size()) - 1;
    m_currentDirectory = newPath;
}

void AssetManagerPanel::RenderNavigationBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

    // History Back Button
    bool canGoBack = (m_historyIndex > 0);
    if (!canGoBack) ImGui::BeginDisabled();
    if (ImGui::Button("<", ImVec2(26.0f, 26.0f))) {
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
    if (ImGui::Button(">", ImVec2(26.0f, 26.0f))) {
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
    if (ImGui::Button("^ Up", ImVec2(48.0f, 26.0f))) {
        if (canGoUp) {
            NavigateTo(m_currentDirectory.parent_path());
        }
    }
    if (!canGoUp) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Parent Directory");

    ImGui::SameLine();

    // Interactive Breadcrumb Path Bar
    UIWidgets::DrawBreadcrumbBar(m_currentDirectory, m_rootDirectory, [this](const std::filesystem::path& target) {
        NavigateTo(target);
    });

    ImGui::SameLine(ImGui::GetWindowWidth() - 320.0f);

    // Search Filter Input
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_searchFilter, sizeof(m_searchFilter));

    if (strlen(m_searchFilter) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("x", ImVec2(22.0f, 24.0f))) {
            m_searchFilter[0] = '\0';
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear search");
    }

    ImGui::SameLine();

    // View Mode Toggle (Grid vs List)
    bool isGrid = (m_viewMode == AssetViewMode::Grid);
    if (VectorIcons::IconButton("##viewModeToggle", isGrid ? VectorIconType::Grid : VectorIconType::List,
                               false, isGrid ? "Switch to List View" : "Switch to Grid View", ImVec2(28.0f, 26.0f))) {
        m_viewMode = isGrid ? AssetViewMode::List : AssetViewMode::Grid;
    }

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

void AssetManagerPanel::RenderCategoryFilterBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

    struct CategoryTab {
        ui::AssetCategory category;
        const char* label;
    };

    CategoryTab tabs[] = {
        { ui::AssetCategory::All,       "All" },
        { ui::AssetCategory::Model3D,   "Models" },
        { ui::AssetCategory::Texture2D, "Textures" },
        { ui::AssetCategory::Material,  "Materials" },
        { ui::AssetCategory::Shader,    "Shaders" },
        { ui::AssetCategory::Scene,     "Scenes" },
        { ui::AssetCategory::Document,  "Docs" }
    };

    for (size_t i = 0; i < IM_ARRAYSIZE(tabs); ++i) {
        if (i > 0) ImGui::SameLine();
        bool isSelected = (m_categoryFilter == tabs[i].category);
        if (UIWidgets::DrawFilterChip(tabs[i].label, isSelected)) {
            m_categoryFilter = tabs[i].category;
        }
    }

    ImGui::PopStyleVar();
    ImGui::Spacing();
}

std::vector<std::filesystem::directory_entry> AssetManagerPanel::GetFilteredDirectoryEntries() {
    std::vector<std::filesystem::directory_entry> entries;
    if (!std::filesystem::exists(m_currentDirectory)) return entries;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory)) {
            // 1. Text Search Filter
            if (strlen(m_searchFilter) > 0) {
                std::string filename = entry.path().filename().string();
                std::string filterStr(m_searchFilter);
                auto it = std::search(filename.begin(), filename.end(), filterStr.begin(), filterStr.end(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                if (it == filename.end()) continue;
            }

            // 2. Category Filter (Directories always pass through)
            if (!entry.is_directory() && m_categoryFilter != ui::AssetCategory::All) {
                ui::AssetCategory itemCat = ui::ThumbnailCache::GetCategoryFromPath(entry.path());
                if (itemCat != m_categoryFilter) continue;
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

    return entries;
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

    ImVec2 treeCursor = ImGui::GetCursorScreenPos();
    bool opened = ImGui::TreeNodeEx(dirPath.string().c_str(), flags, "    %s", folderName.c_str());
    ImVec2 iconCenter(treeCursor.x + (hasSubdirs ? 22.0f : 10.0f), treeCursor.y + 10.0f);
    VectorIcons::Draw(ImGui::GetWindowDrawList(), VectorIconType::Folder, iconCenter, 13.0f,
                      ImGui::GetColorU32(ui::Theme::COLOR_ACCENT_PRIMARY), opened);

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

void AssetManagerPanel::RenderFileGrid(const std::vector<std::filesystem::directory_entry>& entries) {
    float availWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = 108.0f;
    float cardHeight = 118.0f;
    float spacing = 10.0f;

    int columns = static_cast<int>((availWidth + spacing) / (cardWidth + spacing));
    if (columns < 1) columns = 1;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& itemPath = entry.path();
        std::string filename = itemPath.filename().string();
        bool isDir = entry.is_directory();
        bool isSelected = (itemPath == m_selectedPath);

        const auto& thumb = m_thumbnailCache ? m_thumbnailCache->GetOrCreateThumbnail(itemPath) : ui::AssetThumbnail{};
        VectorIconType vIcon = isDir ? VectorIconType::Folder : thumb.vectorIcon;
        ImU32 iconCol = isDir ? ImGui::GetColorU32(ui::Theme::COLOR_TEXT_PRIMARY) : ImGui::GetColorU32(thumb.badgeBgColor);

        if (i % columns != 0) {
            ImGui::SameLine(0, spacing);
        }

        ImGui::PushID(static_cast<int>(i));

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 cardMin = cursor;
        ImVec2 cardMax = ImVec2(cursor.x + cardWidth, cursor.y + cardHeight);

        // Invisible button to handle clicks and hover states
        std::string btnId = "##card_" + std::to_string(i);
        bool clicked = ImGui::InvisibleButton(btnId.c_str(), ImVec2(cardWidth, cardHeight));
        bool isHovered = ImGui::IsItemHovered();

        // 1. Draw Elevated Card Background & Border
        UIWidgets::DrawCard(drawList, cardMin, cardMax, isSelected, isHovered, 8.0f);

        // 2. Draw Top Thumbnail / Vector Icon Area
        float iconAreaHeight = 60.0f;
        ImVec2 iconCenterPos = ImVec2(cardMin.x + cardWidth * 0.5f, cardMin.y + iconAreaHeight * 0.5f);
        VectorIcons::Draw(drawList, vIcon, iconCenterPos, 28.0f, iconCol);

        // 3. Draw Format Badge in Top-Right
        if (!isDir && !thumb.badgeText.empty()) {
            ui::Theme::PushFontSmall();
            ImVec2 badgeTextSize = ImGui::CalcTextSize(thumb.badgeText.c_str());
            ImVec2 badgePos = ImVec2(cardMax.x - badgeTextSize.x - 10.0f, cardMin.y + 6.0f);
            drawList->AddRectFilled(ImVec2(badgePos.x - 3.0f, badgePos.y - 1.0f),
                                   ImVec2(badgePos.x + badgeTextSize.x + 3.0f, badgePos.y + badgeTextSize.y + 1.0f),
                                   ImGui::GetColorU32(thumb.badgeBgColor), 3.0f);
            drawList->AddText(badgePos, ImGui::GetColorU32(thumb.badgeTextColor), thumb.badgeText.c_str());
            ui::Theme::PopFont();
        }

        // 4. Draw Filename Text (with truncation)
        float textY = cardMin.y + iconAreaHeight + 4.0f;
        std::string truncatedName = filename;
        if (truncatedName.length() > 13) {
            truncatedName = truncatedName.substr(0, 10) + "...";
        }
        ImVec2 nameSize = ImGui::CalcTextSize(truncatedName.c_str());
        drawList->AddText(ImVec2(cardMin.x + (cardWidth - nameSize.x) * 0.5f, textY),
                          ImGui::GetColorU32(isSelected ? ui::Theme::COLOR_TEXT_PRIMARY : ui::Theme::COLOR_TEXT_SECONDARY),
                          truncatedName.c_str());

        // 5. Draw File Size Subtitle
        uintmax_t sizeBytes = 0;
        try {
            if (!isDir) sizeBytes = std::filesystem::file_size(itemPath);
        } catch (...) {}

        std::string sizeStr;
        if (isDir) {
            sizeStr = "Folder";
        } else if (sizeBytes < 1024) {
            sizeStr = std::to_string(sizeBytes) + " B";
        } else if (sizeBytes < 1024 * 1024) {
            sizeStr = std::to_string(sizeBytes / 1024) + " KB";
        } else {
            sizeStr = std::to_string(sizeBytes / (1024 * 1024)) + " MB";
        }

        ui::Theme::PushFontSmall();
        ImVec2 sizeTextSize = ImGui::CalcTextSize(sizeStr.c_str());
        drawList->AddText(ImVec2(cardMin.x + (cardWidth - sizeTextSize.x) * 0.5f, textY + 18.0f),
                          ImGui::GetColorU32(ui::Theme::COLOR_TEXT_MUTED), sizeStr.c_str());
        ui::Theme::PopFont();

        if (clicked) {
            m_selectedPath = itemPath;
        }

        if (isHovered) {
            ImGui::SetTooltip("%s\nType: %s\nSize: %s", filename.c_str(),
                              isDir ? "Directory" : thumb.badgeText.c_str(), sizeStr.c_str());
        }

        if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (isDir) {
                NavigateTo(itemPath);
            } else if (thumb.category == ui::AssetCategory::Model3D && m_onOpenModel) {
                m_onOpenModel(itemPath.string());
            }
        }

        // Drag and Drop Source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            std::string pathStr = itemPath.string();
            ImGui::SetDragDropPayload("ASSET_PATH_PAYLOAD", pathStr.c_str(), pathStr.size() + 1);
            VectorIcons::RenderInline(vIcon, 16.0f, isDir ? ui::Theme::COLOR_TEXT_PRIMARY : thumb.badgeBgColor);
            ImGui::SameLine();
            ImGui::Text("%s", filename.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop Target onto Folder Card
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

        // Context Menu
        if (ImGui::BeginPopupContextItem("ItemContext")) {
            m_actionTargetPath = itemPath;
            m_selectedPath = itemPath;

            if (!isDir && thumb.category == ui::AssetCategory::Model3D) {
                if (ImGui::MenuItem("Open Scene with Model")) {
                    if (m_onOpenModel) m_onOpenModel(itemPath.string());
                }
                if (ImGui::MenuItem("Import Model into Scene")) {
                    if (m_onImportModel) m_onImportModel(itemPath.string());
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Rename", "F2")) {
                strncpy(m_nameBuffer, filename.c_str(), sizeof(m_nameBuffer) - 1);
                m_nameBuffer[sizeof(m_nameBuffer) - 1] = '\0';
                m_showRenameModal = true;
            }

            if (ImGui::MenuItem("Delete", "Del")) {
                m_showDeleteModal = true;
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void AssetManagerPanel::RenderFileList(const std::vector<std::filesystem::directory_entry>& entries) {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("AssetListTable", 4, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.45f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            const auto& itemPath = entry.path();
            std::string filename = itemPath.filename().string();
            bool isDir = entry.is_directory();
            bool isSelected = (itemPath == m_selectedPath);

            const auto& thumb = m_thumbnailCache ? m_thumbnailCache->GetOrCreateThumbnail(itemPath) : ui::AssetThumbnail{};
            VectorIconType vIcon = isDir ? VectorIconType::Folder : thumb.vectorIcon;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushID(static_cast<int>(i));

            ImVec2 rowCursor = ImGui::GetCursorScreenPos();
            std::string rowLabel = "      " + filename;

            if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selectedPath = itemPath;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (isDir) {
                        NavigateTo(itemPath);
                    } else if (thumb.category == ui::AssetCategory::Model3D && m_onOpenModel) {
                        m_onOpenModel(itemPath.string());
                    }
                }
            }

            ImVec2 rowIconCenter(rowCursor.x + 10.0f, rowCursor.y + 10.0f);
            ImU32 rowIconCol = isDir ? ImGui::GetColorU32(ui::Theme::COLOR_TEXT_PRIMARY) : ImGui::GetColorU32(thumb.badgeBgColor);
            VectorIcons::Draw(ImGui::GetWindowDrawList(), vIcon, rowIconCenter, 14.0f, rowIconCol);

            // Drag and Drop Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string pathStr = itemPath.string();
                ImGui::SetDragDropPayload("ASSET_PATH_PAYLOAD", pathStr.c_str(), pathStr.size() + 1);
                VectorIcons::RenderInline(vIcon, 16.0f, isDir ? ui::Theme::COLOR_TEXT_PRIMARY : thumb.badgeBgColor);
                ImGui::SameLine();
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            // Context Menu
            if (ImGui::BeginPopupContextItem("ItemContextList")) {
                m_actionTargetPath = itemPath;
                m_selectedPath = itemPath;

                if (!isDir && thumb.category == ui::AssetCategory::Model3D) {
                    if (ImGui::MenuItem("Open Scene with Model")) {
                        if (m_onOpenModel) m_onOpenModel(itemPath.string());
                    }
                    if (ImGui::MenuItem("Import Model into Scene")) {
                        if (m_onImportModel) m_onImportModel(itemPath.string());
                    }
                    ImGui::Separator();
                }

                if (ImGui::MenuItem("Rename")) {
                    strncpy(m_nameBuffer, filename.c_str(), sizeof(m_nameBuffer) - 1);
                    m_nameBuffer[sizeof(m_nameBuffer) - 1] = '\0';
                    m_showRenameModal = true;
                }

                if (ImGui::MenuItem("Delete")) {
                    m_showDeleteModal = true;
                }

                ImGui::EndPopup();
            }

            // Column 2: Type
            ImGui::TableNextColumn();
            if (isDir) {
                ImGui::TextDisabled("Folder");
            } else {
                UIWidgets::DrawBadge(thumb.badgeText.c_str(), thumb.badgeBgColor, thumb.badgeTextColor);
            }

            // Column 3: Size
            ImGui::TableNextColumn();
            uintmax_t sizeBytes = 0;
            try {
                if (!isDir) sizeBytes = std::filesystem::file_size(itemPath);
            } catch (...) {}

            if (isDir) {
                ImGui::TextDisabled("-");
            } else if (sizeBytes < 1024) {
                ImGui::Text("%zu B", sizeBytes);
            } else if (sizeBytes < 1024 * 1024) {
                ImGui::Text("%.1f KB", sizeBytes / 1024.0f);
            } else {
                ImGui::Text("%.1f MB", sizeBytes / (1024.0f * 1024.0f));
            }

            // Column 4: Last Modified
            ImGui::TableNextColumn();
            try {
                auto ftime = std::filesystem::last_write_time(itemPath);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                std::tm tmBuffer;
#if defined(_WIN32)
                localtime_s(&tmBuffer, &cftime);
#else
                localtime_r(&cftime, &tmBuffer);
#endif
                char timeStr[64];
                std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tmBuffer);
                ImGui::TextDisabled("%s", timeStr);
            } catch (...) {
                ImGui::TextDisabled("-");
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
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
        ImGui::TextColored(ui::Theme::COLOR_ERROR, "Are you sure you want to delete '%s'?",
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
    RenderCategoryFilterBar();

    ImGui::Columns(2, "AssetManagerSplit", true);
    ImGui::SetColumnWidth(0, 220.0f);

    // Left Column: Directory Tree
    ui::Theme::PushFontHeader();
    ImGui::TextColored(ui::Theme::COLOR_TEXT_PRIMARY, "Folders");
    ui::Theme::PopFont();
    ImGui::Separator();
    ImGui::BeginChild("FolderTreeScroll", ImVec2(0, 0), false);
    RenderFolderTree(m_rootDirectory);
    ImGui::EndChild();

    ImGui::NextColumn();

    // Right Column: File Grid or Table
    ui::Theme::PushFontHeader();
    ImGui::TextColored(ui::Theme::COLOR_TEXT_PRIMARY, "Contents");
    ui::Theme::PopFont();
    ImGui::Separator();

    auto entries = GetFilteredDirectoryEntries();

    ImGui::BeginChild("FileGridScroll", ImVec2(0, 0), false);
    if (m_viewMode == AssetViewMode::Grid) {
        RenderFileGrid(entries);
    } else {
        RenderFileList(entries);
    }
    ImGui::EndChild();

    // Context menu on empty background area
    if (ImGui::BeginPopupContextWindow("GridBackgroundContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("+ New Folder")) {
            m_nameBuffer[0] = '\0';
            m_showCreateFolderModal = true;
        }
        if (ImGui::MenuItem("+ New Material (.mat)")) {
            strncpy(m_nameBuffer, "NewMaterial.mat", sizeof(m_nameBuffer) - 1);
            m_showCreateMaterialModal = true;
        }
        if (ImGui::MenuItem("+ New Text File (.txt)")) {
            strncpy(m_nameBuffer, "NewFile.txt", sizeof(m_nameBuffer) - 1);
            m_showCreateFileModal = true;
        }
        ImGui::EndPopup();
    }

    ImGui::Columns(1);

    RenderContextMenu();

    ImGui::End();
}

} // namespace khepri

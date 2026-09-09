#include "AssetManagerPanel.h"
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

AssetManagerPanel::AssetManagerPanel(ui::ThumbnailCache* thumbnailCache)
    : m_thumbnailCache(thumbnailCache) {
    if (!m_thumbnailCache) {
        m_ownedThumbnailCache = std::make_unique<ui::ThumbnailCache>();
        m_thumbnailCache = m_ownedThumbnailCache.get();
    }

    m_rootDirectory = ui::Theme::FindAssetDirectory();
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
    RefreshCache();
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
    InvalidateCache();
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
            InvalidateCache();
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
            InvalidateCache();
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

    float scale = Theme::GetTotalScale();
    float searchWidth = 160.0f * scale;
    float rightControlsWidth = searchWidth + 70.0f * scale;
    float availWidth = ImGui::GetWindowWidth();
    if (availWidth > rightControlsWidth + 240.0f * scale) {
        ImGui::SameLine(availWidth - rightControlsWidth);
    } else {
        ImGui::SameLine();
    }

    // Search Filter Input
    ImGui::SetNextItemWidth(searchWidth);
    if (ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_searchFilter, sizeof(m_searchFilter))) {
        UpdateFilteredEntries();
    }

    if (strlen(m_searchFilter) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("x", ImVec2(22.0f * scale, 24.0f * scale))) {
            m_searchFilter[0] = '\0';
            UpdateFilteredEntries();
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

static std::string TruncateUtf8(const std::string& str, size_t maxChars) {
    size_t count = 0;
    size_t byteIndex = 0;
    while (byteIndex < str.size() && count < maxChars) {
        unsigned char c = static_cast<unsigned char>(str[byteIndex]);
        if ((c & 0x80) == 0) byteIndex += 1;
        else if ((c & 0xE0) == 0xC0) byteIndex += 2;
        else if ((c & 0xF0) == 0xE0) byteIndex += 3;
        else if ((c & 0xF8) == 0xF0) byteIndex += 4;
        else byteIndex += 1;
        count++;
    }
    if (byteIndex < str.size()) {
        return str.substr(0, byteIndex) + "...";
    }
    return str;
}

void AssetManagerPanel::RefreshCache() {
    m_cachedEntries.clear();
    if (!std::filesystem::exists(m_currentDirectory)) {
        m_cacheDirty = false;
        m_lastCacheRefresh = std::chrono::steady_clock::now();
        UpdateFilteredEntries();
        return;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory)) {
            CachedAssetEntry item;
            item.path = entry.path();
            item.filename = entry.path().filename().string();
            item.isDirectory = entry.is_directory();

            if (!item.isDirectory) {
                try {
                    item.fileSize = std::filesystem::file_size(entry.path());
                } catch (...) {
                    item.fileSize = 0;
                }
                item.category = ui::ThumbnailCache::GetCategoryFromPath(entry.path());

                if (item.fileSize < 1024) {
                    item.formattedSize = std::to_string(item.fileSize) + " B";
                } else if (item.fileSize < 1024 * 1024) {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(1) << (item.fileSize / 1024.0f) << " KB";
                    item.formattedSize = ss.str();
                } else {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(1) << (item.fileSize / (1024.0f * 1024.0f)) << " MB";
                    item.formattedSize = ss.str();
                }
            } else {
                item.category = ui::AssetCategory::Unknown;
                item.fileSize = 0;
                item.formattedSize = "Folder";
            }

            try {
                auto ftime = std::filesystem::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                item.lastWriteTime = std::chrono::system_clock::to_time_t(sctp);
                std::tm tmBuffer;
#if defined(_WIN32)
                localtime_s(&tmBuffer, &item.lastWriteTime);
#else
                localtime_r(&item.lastWriteTime, &tmBuffer);
#endif
                char timeStr[64];
                std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tmBuffer);
                item.formattedTime = timeStr;
            } catch (...) {
                item.lastWriteTime = 0;
                item.formattedTime = "-";
            }

            m_cachedEntries.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error refreshing asset cache: " + std::string(e.what()));
    }

    // Default sort: directories first, then alphabetically by filename
    std::sort(m_cachedEntries.begin(), m_cachedEntries.end(), [](const CachedAssetEntry& a, const CachedAssetEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory;
        }
        return a.filename < b.filename;
    });

    m_cacheDirty = false;
    m_lastCacheRefresh = std::chrono::steady_clock::now();
    UpdateFilteredEntries();
}

void AssetManagerPanel::UpdateFilteredEntries() {
    m_filteredEntries.clear();
    std::string filterStr(m_searchFilter);
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& entry : m_cachedEntries) {
        // 1. Text Search Filter
        if (!filterStr.empty()) {
            std::string lowerFilename = entry.filename;
            std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowerFilename.find(filterStr) == std::string::npos) {
                continue;
            }
        }

        // 2. Category Filter (Directories always pass through)
        if (!entry.isDirectory && m_categoryFilter != ui::AssetCategory::All) {
            if (entry.category != m_categoryFilter) {
                continue;
            }
        }

        m_filteredEntries.push_back(entry);
    }
}

const std::vector<AssetManagerPanel::CachedAssetEntry>& AssetManagerPanel::GetFilteredEntries() {
    auto now = std::chrono::steady_clock::now();
    if (m_cacheDirty || (now - m_lastCacheRefresh) > std::chrono::milliseconds(1000)) {
        RefreshCache();
    }
    return m_filteredEntries;
}

void AssetManagerPanel::RenderFolderTree(const std::filesystem::path& dirPath) {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) return;

    ImGui::PushID(dirPath.string().c_str());

    float scale = Theme::GetTotalScale();
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
    bool opened = ImGui::TreeNodeEx("##Node", flags, "    %s", folderName.c_str());
    ImVec2 iconCenter(treeCursor.x + (hasSubdirs ? 22.0f : 10.0f) * scale, treeCursor.y + 10.0f * scale);
    VectorIcons::Draw(ImGui::GetWindowDrawList(), VectorIconType::Folder, iconCenter, 13.0f * scale,
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
                if (std::filesystem::exists(targetPath)) {
                    LOG_WARNING("Cannot move asset: destination already exists: " + targetPath.string());
                } else {
                    try {
                        std::filesystem::rename(sourcePath, targetPath);
                        LOG_INFO("Moved asset to: " + targetPath.string());
                        InvalidateCache();
                    } catch (const std::exception& e) {
                        LOG_ERROR("Failed to move asset: " + std::string(e.what()));
                    }
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

    ImGui::PopID();
}

void AssetManagerPanel::RenderFileGrid(const std::vector<CachedAssetEntry>& entries) {
    float scale = Theme::GetTotalScale();
    float availWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = 108.0f * scale;
    float cardHeight = 118.0f * scale;
    float spacing = 10.0f * scale;

    int columns = static_cast<int>((availWidth + spacing) / (cardWidth + spacing));
    if (columns < 1) columns = 1;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& itemPath = entry.path;
        const std::string& filename = entry.filename;
        bool isDir = entry.isDirectory;
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
        UIWidgets::DrawCard(drawList, cardMin, cardMax, isSelected, isHovered, 8.0f * scale);

        // 2. Draw Top Thumbnail / Vector Icon Area
        float iconAreaHeight = 60.0f * scale;
        ImVec2 iconCenterPos = ImVec2(cardMin.x + cardWidth * 0.5f, cardMin.y + iconAreaHeight * 0.5f);
        VectorIcons::Draw(drawList, vIcon, iconCenterPos, 28.0f * scale, iconCol);

        // 3. Draw Format Badge in Top-Right
        if (!isDir && !thumb.badgeText.empty()) {
            ui::Theme::PushFontSmall();
            ImVec2 badgeTextSize = ImGui::CalcTextSize(thumb.badgeText.c_str());
            ImVec2 badgePos = ImVec2(cardMax.x - badgeTextSize.x - 10.0f * scale, cardMin.y + 6.0f * scale);
            drawList->AddRectFilled(ImVec2(badgePos.x - 3.0f * scale, badgePos.y - 1.0f * scale),
                                   ImVec2(badgePos.x + badgeTextSize.x + 3.0f * scale, badgePos.y + badgeTextSize.y + 1.0f * scale),
                                   ImGui::GetColorU32(thumb.badgeBgColor), 3.0f * scale);
            drawList->AddText(badgePos, ImGui::GetColorU32(thumb.badgeTextColor), thumb.badgeText.c_str());
            ui::Theme::PopFont();
        }

        // 4. Draw Filename Text (with safe UTF-8 truncation)
        float textY = cardMin.y + iconAreaHeight + 4.0f * scale;
        std::string truncatedName = TruncateUtf8(filename, 12);
        ImVec2 nameSize = ImGui::CalcTextSize(truncatedName.c_str());
        drawList->AddText(ImVec2(cardMin.x + (cardWidth - nameSize.x) * 0.5f, textY),
                          ImGui::GetColorU32(isSelected ? ui::Theme::COLOR_TEXT_PRIMARY : ui::Theme::COLOR_TEXT_SECONDARY),
                          truncatedName.c_str());

        // 5. Draw File Size Subtitle (from cache)
        ui::Theme::PushFontSmall();
        ImVec2 sizeTextSize = ImGui::CalcTextSize(entry.formattedSize.c_str());
        drawList->AddText(ImVec2(cardMin.x + (cardWidth - sizeTextSize.x) * 0.5f, textY + 18.0f * scale),
                          ImGui::GetColorU32(ui::Theme::COLOR_TEXT_MUTED), entry.formattedSize.c_str());
        ui::Theme::PopFont();

        if (clicked) {
            m_selectedPath = itemPath;
        }

        if (isHovered) {
            ImGui::SetTooltip("%s\nType: %s\nSize: %s", filename.c_str(),
                              isDir ? "Directory" : thumb.badgeText.c_str(), entry.formattedSize.c_str());
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
            VectorIcons::RenderInline(vIcon, 16.0f * scale, isDir ? ui::Theme::COLOR_TEXT_PRIMARY : thumb.badgeBgColor);
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
                    if (std::filesystem::exists(targetPath)) {
                        LOG_WARNING("Cannot move asset: destination already exists: " + targetPath.string());
                    } else {
                        try {
                            std::filesystem::rename(sourcePath, targetPath);
                            LOG_INFO("Moved asset to: " + targetPath.string());
                            InvalidateCache();
                        } catch (const std::exception& e) {
                            LOG_ERROR("Failed to move asset: " + std::string(e.what()));
                        }
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

void AssetManagerPanel::RenderFileList(const std::vector<CachedAssetEntry>& entries) {
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

        ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
        if (sortSpecs && sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0) {
            const ImGuiTableColumnSortSpecs* spec = &sortSpecs->Specs[0];
            bool ascending = (spec->SortDirection == ImGuiSortDirection_Ascending);
            int colIndex = spec->ColumnIndex;

            std::sort(m_filteredEntries.begin(), m_filteredEntries.end(), [colIndex, ascending](const CachedAssetEntry& a, const CachedAssetEntry& b) {
                if (a.isDirectory != b.isDirectory) {
                    return a.isDirectory;
                }
                int cmp = 0;
                switch (colIndex) {
                    case 0: // Name
                        cmp = a.filename.compare(b.filename);
                        break;
                    case 1: // Type
                        cmp = static_cast<int>(a.category) - static_cast<int>(b.category);
                        break;
                    case 2: // Size
                        if (a.fileSize < b.fileSize) cmp = -1;
                        else if (a.fileSize > b.fileSize) cmp = 1;
                        break;
                    case 3: // Modified
                        if (a.lastWriteTime < b.lastWriteTime) cmp = -1;
                        else if (a.lastWriteTime > b.lastWriteTime) cmp = 1;
                        break;
                    default:
                        cmp = a.filename.compare(b.filename);
                        break;
                }
                return ascending ? (cmp < 0) : (cmp > 0);
            });
            sortSpecs->SpecsDirty = false;
        }

        float scale = Theme::GetTotalScale();

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            const auto& itemPath = entry.path;
            const std::string& filename = entry.filename;
            bool isDir = entry.isDirectory;
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

            ImVec2 rowIconCenter(rowCursor.x + 10.0f * scale, rowCursor.y + 10.0f * scale);
            ImU32 rowIconCol = isDir ? ImGui::GetColorU32(ui::Theme::COLOR_TEXT_PRIMARY) : ImGui::GetColorU32(thumb.badgeBgColor);
            VectorIcons::Draw(ImGui::GetWindowDrawList(), vIcon, rowIconCenter, 14.0f * scale, rowIconCol);

            // Drag and Drop Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string pathStr = itemPath.string();
                ImGui::SetDragDropPayload("ASSET_PATH_PAYLOAD", pathStr.c_str(), pathStr.size() + 1);
                VectorIcons::RenderInline(vIcon, 16.0f * scale, isDir ? ui::Theme::COLOR_TEXT_PRIMARY : thumb.badgeBgColor);
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
            if (isDir) {
                ImGui::TextDisabled("-");
            } else {
                ImGui::Text("%s", entry.formattedSize.c_str());
            }

            // Column 4: Last Modified
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", entry.formattedTime.c_str());

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
                    InvalidateCache();
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

                auto mat = AssetManager::Instance().CreateMaterial(newMatFile.stem().string());
                if (mat) {
                    AssetManager::Instance().SaveMaterial(*mat, newMatFile);
                    LOG_INFO("Created material file: " + newMatFile.string());
                }
                InvalidateCache();
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
                    InvalidateCache();
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
                    InvalidateCache();
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
                InvalidateCache();
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

    const auto& entries = GetFilteredEntries();

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

#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include "Theme.h"

namespace khepri::ui {

enum class DropPosition {
    None,
    Above,
    Inside,
    Below
};

/**
 * @brief Custom ImDrawList drawing primitives and high-level UI widgets.
 */
class UIWidgets {
public:
    /**
     * @brief Renders a card container with elevation background, rounded corners,
     * optional 1px border, and selection halo.
     */
    static void DrawCard(ImDrawList* drawList, const ImVec2& minPos, const ImVec2& maxPos,
                         bool isSelected, bool isHovered, float rounding = 6.0f);

    /**
     * @brief Renders a micro badge / chip with text and custom background/foreground colors.
     */
    static void DrawBadge(const char* text, const ImVec4& bgColor, const ImVec4& textColor,
                          float rounding = 4.0f);

    /**
     * @brief Renders an interactive breadcrumb path bar with clickable segments.
     */
    static void DrawBreadcrumbBar(const std::filesystem::path& currentPath,
                                  const std::filesystem::path& rootPath,
                                  const std::function<void(const std::filesystem::path&)>& onNavigate);

    /**
     * @brief Renders subtle tree branch connector guide lines (Godot-style).
     */
    static void DrawTreeConnectorLine(ImDrawList* drawList, const ImVec2& parentPos,
                                      const ImVec2& nodePos, bool isLastChild);

    /**
     * @brief Renders a drop insertion indicator line with a circular anchor.
     */
    static void DrawDropIndicator(ImDrawList* drawList, const ImVec2& startPos,
                                  const ImVec2& endPos, DropPosition position);

    /**
     * @brief Renders a compact hover-activated icon button (e.g. eye toggle, lock toggle).
     * @return true if clicked.
     */
    static bool DrawIconButton(const char* strId, const char* iconText, bool active,
                               const char* tooltip = nullptr, const ImVec2& size = ImVec2(20.0f, 20.0f));

    /**
     * @brief Renders a subtle selection halo or focus ring around an area.
     */
    static void DrawFocusRing(ImDrawList* drawList, const ImVec2& minPos, const ImVec2& maxPos,
                              const ImVec4& color = Theme::COLOR_ACCENT_PRIMARY, float rounding = 6.0f);

    /**
     * @brief Renders a category filter button tab with count badge.
     */
    static bool DrawFilterChip(const char* label, bool isSelected, int count = -1);
};

} // namespace khepri::ui

#include "UIWidgets.h"
#include "Icons.h"
#include <imgui_internal.h>
#include <algorithm>

namespace khepri::ui {

void UIWidgets::DrawCard(ImDrawList* drawList, const ImVec2& minPos, const ImVec2& maxPos,
                         bool isSelected, bool isHovered, float rounding) {
    if (!drawList) drawList = ImGui::GetWindowDrawList();

    ImU32 bgColor = ImGui::GetColorU32(Theme::COLOR_CARD_BG);
    ImU32 borderColor = ImGui::GetColorU32(Theme::COLOR_BORDER_SUBTLE);

    if (isSelected) {
        bgColor = ImGui::GetColorU32(Theme::COLOR_CARD_SELECTED);
        borderColor = ImGui::GetColorU32(Theme::COLOR_ACCENT_PRIMARY);
    } else if (isHovered) {
        bgColor = ImGui::GetColorU32(Theme::COLOR_CARD_HOVER);
        borderColor = ImGui::GetColorU32(Theme::COLOR_BORDER);
    }

    // 1. Background Fill
    drawList->AddRectFilled(minPos, maxPos, bgColor, rounding);

    // 2. 1px Border Outline
    drawList->AddRect(minPos, maxPos, borderColor, rounding, 0, 1.0f);

    // 3. Selection Accent Glow
    if (isSelected) {
        ImVec2 glowMin = ImVec2(minPos.x - 1.0f, minPos.y - 1.0f);
        ImVec2 glowMax = ImVec2(maxPos.x + 1.0f, maxPos.y + 1.0f);
        ImU32 glowColor = ImGui::GetColorU32(ImVec4(0.231f, 0.510f, 0.965f, 0.40f));
        drawList->AddRect(glowMin, glowMax, glowColor, rounding + 1.0f, 0, 1.5f);
    }
}

void UIWidgets::DrawBadge(const char* text, const ImVec4& bgColor, const ImVec4& textColor,
                          float rounding) {
    Theme::PushFontSmall();

    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 padding(6.0f, 2.0f);
    ImVec2 badgeSize(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 minPos = cursor;
    ImVec2 maxPos = ImVec2(cursor.x + badgeSize.x, cursor.y + badgeSize.y);

    drawList->AddRectFilled(minPos, maxPos, ImGui::GetColorU32(bgColor), rounding);
    drawList->AddRect(minPos, maxPos, ImGui::GetColorU32(ImVec4(textColor.x, textColor.y, textColor.z, 0.4f)), rounding, 0, 1.0f);

    ImVec2 textPos = ImVec2(cursor.x + padding.x, cursor.y + padding.y);
    drawList->AddText(textPos, ImGui::GetColorU32(textColor), text);

    ImGui::Dummy(badgeSize);
    Theme::PopFont();
}

void UIWidgets::DrawBreadcrumbBar(const std::filesystem::path& currentPath,
                                  const std::filesystem::path& rootPath,
                                  const std::function<void(const std::filesystem::path&)>& onNavigate) {
    // Build path segments relative to rootPath's parent
    std::filesystem::path anchor = rootPath.parent_path().empty() ? rootPath : rootPath.parent_path();
    std::filesystem::path rel = std::filesystem::relative(currentPath, anchor);

    std::vector<std::pair<std::string, std::filesystem::path>> segments;
    std::filesystem::path accum = anchor;

    for (const auto& part : rel) {
        accum /= part;
        segments.emplace_back(part.string(), accum);
    }

    if (segments.empty()) {
        segments.emplace_back("assets", rootPath);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.25f, 0.35f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.231f, 0.510f, 0.965f, 0.3f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0, 2.0f);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0, 2.0f);
        }

        bool isLast = (i == segments.size() - 1);
        if (isLast) {
            ImGui::TextColored(Theme::COLOR_TEXT_PRIMARY, "%s", segments[i].first.c_str());
        } else {
            std::string btnLabel = segments[i].first + "##crumb_" + std::to_string(i);
            if (ImGui::Button(btnLabel.c_str())) {
                if (onNavigate) {
                    onNavigate(segments[i].second);
                }
            }
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

void UIWidgets::DrawTreeConnectorLine(ImDrawList* drawList, const ImVec2& parentPos,
                                      const ImVec2& nodePos, bool isLastChild) {
    if (!drawList) drawList = ImGui::GetWindowDrawList();

    ImU32 lineColor = ImGui::GetColorU32(Theme::COLOR_BORDER_SUBTLE);
    float lineThickness = 1.0f;

    // Vertical guide line from parent down to node center
    drawList->AddLine(ImVec2(parentPos.x, parentPos.y),
                      ImVec2(parentPos.x, nodePos.y),
                      lineColor, lineThickness);

    // Horizontal guide line connecting into node
    drawList->AddLine(ImVec2(parentPos.x, nodePos.y),
                      ImVec2(nodePos.x, nodePos.y),
                      lineColor, lineThickness);

    (void)isLastChild;
}

void UIWidgets::DrawDropIndicator(ImDrawList* drawList, const ImVec2& startPos,
                                  const ImVec2& endPos, DropPosition position) {
    if (!drawList) drawList = ImGui::GetWindowDrawList();
    if (position == DropPosition::None) return;

    ImU32 indicatorColor = ImGui::GetColorU32(Theme::COLOR_ACCENT_CYAN);

    if (position == DropPosition::Inside) {
        // Draw outline around the inside target node
        drawList->AddRect(startPos, endPos, indicatorColor, 4.0f, 0, 1.5f);
    } else {
        // Draw horizontal drop line
        float y = (position == DropPosition::Above) ? startPos.y : endPos.y;
        drawList->AddLine(ImVec2(startPos.x, y), ImVec2(endPos.x, y), indicatorColor, 2.0f);
        // Draw small anchor circle at the left edge
        drawList->AddCircleFilled(ImVec2(startPos.x + 3.0f, y), 3.5f, indicatorColor);
    }
}

bool UIWidgets::DrawIconButton(const char* strId, const char* iconText, bool active,
                              const char* tooltip, const ImVec2& size) {
    ImGui::PushID(strId);

    ImVec4 bgCol = active ? ImVec4(0.231f, 0.510f, 0.965f, 0.25f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec4 textCol = active ? Theme::COLOR_ACCENT_CYAN : Theme::COLOR_TEXT_SECONDARY;

    ImGui::PushStyleColor(ImGuiCol_Button, bgCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.25f, 0.35f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.231f, 0.510f, 0.965f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));

    bool clicked = ImGui::Button(iconText, size);

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();

    return clicked;
}

void UIWidgets::DrawFocusRing(ImDrawList* drawList, const ImVec2& minPos, const ImVec2& maxPos,
                              const ImVec4& color, float rounding) {
    if (!drawList) drawList = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(color);
    drawList->AddRect(minPos, maxPos, col, rounding, 0, 1.5f);
}

bool UIWidgets::DrawFilterChip(const char* label, bool isSelected, int count) {
    ImVec4 bg = isSelected ? ImVec4(0.231f, 0.510f, 0.965f, 0.35f) : ImVec4(0.12f, 0.14f, 0.18f, 0.8f);
    ImVec4 textCol = isSelected ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_SECONDARY;
    ImVec4 border = isSelected ? Theme::COLOR_ACCENT_PRIMARY : Theme::COLOR_BORDER_SUBTLE;

    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.231f, 0.510f, 0.965f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f); // Pill shape
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));

    std::string text = label;
    if (count >= 0) {
        text += " (" + std::to_string(count) + ")";
    }

    bool clicked = ImGui::Button(text.c_str());

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);

    return clicked;
}

} // namespace khepri::ui

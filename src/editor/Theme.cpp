#include "Theme.h"
#include "Icons.h"
#include "EmbeddedFonts.h"
#include "../core/Logger.h"
#include <algorithm>

namespace khepri::ui {

ImFont* Theme::FontDefault = nullptr;
ImFont* Theme::FontTitle   = nullptr;
ImFont* Theme::FontHeader  = nullptr;
ImFont* Theme::FontSmall   = nullptr;
ImFont* Theme::FontMono    = nullptr;

float Theme::s_contentScale = 1.0f;
float Theme::s_userScale    = 1.0f;

void Theme::ApplyTheme(float scale) {
    if (scale <= 0.0f) scale = 1.0f;

    ImGuiStyle& style = ImGui::GetStyle();

    // Spacing & Sizing Tokens
    style.WindowPadding     = ImVec2(10.0f * scale, 10.0f * scale);
    style.FramePadding      = ImVec2(8.0f * scale, 5.0f * scale);
    style.CellPadding       = ImVec2(6.0f * scale, 5.0f * scale);
    style.ItemSpacing       = ImVec2(8.0f * scale, 6.0f * scale);
    style.ItemInnerSpacing  = ImVec2(6.0f * scale, 4.0f * scale);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing     = 16.0f * scale;
    style.ScrollbarSize     = 12.0f * scale;
    style.GrabMinSize       = 10.0f * scale;

    // Borders & Outlines
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    // Rounding Tokens (Modern pill/rounded aesthetics)
    style.WindowRounding    = 8.0f * scale;
    style.ChildRounding     = 6.0f * scale;
    style.FrameRounding     = 6.0f * scale;
    style.PopupRounding     = 8.0f * scale;
    style.ScrollbarRounding = 8.0f * scale;
    style.GrabRounding      = 4.0f * scale;
    style.TabRounding       = 6.0f * scale;

    // Alignment & Behavior
    style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition      = ImGuiDir_Right;
    style.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign      = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize  = 1.0f;
    style.SeparatorTextAlign       = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding     = ImVec2(12.0f * scale, 4.0f * scale);

    // --- Oryzo / Lusion Calibrated Dark Palette ---
    ImVec4* colors = style.Colors;

    // Text
    colors[ImGuiCol_Text]                  = COLOR_TEXT_PRIMARY;
    colors[ImGuiCol_TextDisabled]          = COLOR_TEXT_MUTED;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.231f, 0.510f, 0.965f, 0.35f);

    // Windows & Backgrounds
    colors[ImGuiCol_WindowBg]              = COLOR_BG_DARK;
    colors[ImGuiCol_ChildBg]               = COLOR_PANEL_BG;
    colors[ImGuiCol_PopupBg]               = ImVec4(0.094f, 0.110f, 0.137f, 0.98f);
    
    // Borders
    colors[ImGuiCol_Border]                = COLOR_BORDER;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Frame (Inputs, buttons, checkboxes, text fields)
    colors[ImGuiCol_FrameBg]               = ImVec4(0.118f, 0.137f, 0.169f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.165f, 0.192f, 0.235f, 1.0f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.188f, 0.220f, 0.271f, 1.0f);

    // Title Bar
    colors[ImGuiCol_TitleBg]               = ImVec4(0.071f, 0.082f, 0.102f, 1.0f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.086f, 0.102f, 0.125f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.055f, 0.063f, 0.078f, 0.85f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.071f, 0.082f, 0.102f, 1.0f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.063f, 0.071f, 0.086f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.188f, 0.220f, 0.267f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.255f, 0.302f, 0.365f, 0.85f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.322f, 0.380f, 0.459f, 1.00f);

    // Widgets (Checkmarks, Sliders, Buttons)
    colors[ImGuiCol_CheckMark]             = COLOR_ACCENT_CYAN;
    colors[ImGuiCol_SliderGrab]            = COLOR_ACCENT_PRIMARY;
    colors[ImGuiCol_SliderGrabActive]      = COLOR_ACCENT_HOVER;

    // Buttons
    colors[ImGuiCol_Button]                = ImVec4(0.137f, 0.161f, 0.200f, 1.0f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.188f, 0.224f, 0.278f, 1.0f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.231f, 0.510f, 0.965f, 0.75f);

    // Headers (TreeNodes, Selectables, CollapsingHeaders)
    colors[ImGuiCol_Header]                = ImVec4(0.137f, 0.161f, 0.200f, 0.50f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.188f, 0.224f, 0.278f, 0.75f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.231f, 0.510f, 0.965f, 0.40f);

    // Separators
    colors[ImGuiCol_Separator]             = COLOR_BORDER_SUBTLE;
    colors[ImGuiCol_SeparatorHovered]      = COLOR_BORDER;
    colors[ImGuiCol_SeparatorActive]       = COLOR_ACCENT_PRIMARY;

    // Resize Grips
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.188f, 0.220f, 0.267f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]     = COLOR_ACCENT_PRIMARY;
    colors[ImGuiCol_ResizeGripActive]      = COLOR_ACCENT_HOVER;

    // Tabs
    colors[ImGuiCol_Tab]                   = ImVec4(0.082f, 0.094f, 0.114f, 1.0f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.165f, 0.192f, 0.235f, 1.0f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.122f, 0.141f, 0.173f, 1.0f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.071f, 0.082f, 0.102f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.102f, 0.118f, 0.145f, 1.0f);

    // Docking
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.231f, 0.510f, 0.965f, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.051f, 0.059f, 0.071f, 1.0f);

    // Plot / Graph lines
    colors[ImGuiCol_PlotLines]             = COLOR_ACCENT_CYAN;
    colors[ImGuiCol_PlotLinesHovered]      = COLOR_ACCENT_HOVER;
    colors[ImGuiCol_PlotHistogram]         = COLOR_ACCENT_PRIMARY;
    colors[ImGuiCol_PlotHistogramHovered]  = COLOR_ACCENT_HOVER;

    // Tables
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.094f, 0.110f, 0.137f, 1.0f);
    colors[ImGuiCol_TableBorderStrong]     = COLOR_BORDER;
    colors[ImGuiCol_TableBorderLight]      = COLOR_BORDER_SUBTLE;
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

    // Navigation & Modals
    colors[ImGuiCol_NavHighlight]          = COLOR_ACCENT_CYAN;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);
}

void Theme::LoadFonts(ImGuiIO& io, float scale, const std::string& fontDir) {
    if (scale <= 0.0f) scale = 1.0f;

    io.Fonts->Clear();

    // Scale font sizes
    const float baseSize   = 14.0f * scale;
    const float titleSize  = 18.0f * scale;
    const float headerSize = 16.0f * scale;
    const float smallSize  = 11.0f * scale;
    const float monoSize   = 13.0f * scale;

    // Potential font search paths
    std::vector<std::filesystem::path> fontSearchDirs = {
        std::filesystem::path(fontDir),
        std::filesystem::current_path() / "assets/fonts",
        std::filesystem::current_path() / "../assets/fonts",
        std::filesystem::current_path() / "../../assets/fonts",
        std::filesystem::current_path() / "../../../assets/fonts",
        "C:/Windows/Fonts"
    };

    std::string mainFontPath;
    std::string monoFontPath;

    std::vector<std::string> fontCandidates = {
        "Inter-Regular.ttf", "Inter.ttf", "Geist-Regular.ttf", "PlusJakartaSans-Regular.ttf",
        "segoeui.ttf", "SegoeUI.ttf", "arial.ttf"
    };

    std::vector<std::string> monoCandidates = {
        "JetBrainsMono-Regular.ttf", "FiraCode-Regular.ttf", "CascadiaMono.ttf", "consola.ttf", "Consolas.ttf"
    };

    for (const auto& dir : fontSearchDirs) {
        if (!mainFontPath.empty()) break;
        if (!std::filesystem::exists(dir)) continue;
        for (const auto& f : fontCandidates) {
            auto p = dir / f;
            if (std::filesystem::exists(p)) {
                mainFontPath = p.string();
                break;
            }
        }
    }

    for (const auto& dir : fontSearchDirs) {
        if (!monoFontPath.empty()) break;
        if (!std::filesystem::exists(dir)) continue;
        for (const auto& m : monoCandidates) {
            auto p = dir / m;
            if (std::filesystem::exists(p)) {
                monoFontPath = p.string();
                break;
            }
        }
    }

    auto loadSingleFont = [&](const std::string& fontPath, float size, const char* name) -> ImFont* {
        ImFontConfig fontCfg;
        fontCfg.FontDataOwnedByAtlas = false;
        fontCfg.OversampleH = 3;
        fontCfg.OversampleV = 3;
        fontCfg.RasterizerMultiply = 1.05f;
        strncpy(fontCfg.Name, name, sizeof(fontCfg.Name) - 1);

        ImFont* font = nullptr;
        if (!fontPath.empty() && std::filesystem::exists(fontPath)) {
            font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), size, &fontCfg);
        } else {
            fontCfg.SizePixels = size;
            font = io.Fonts->AddFontDefault(&fontCfg);
        }
        return font;
    };

    // 1. Default UI Font (14px scaled)
    FontDefault = loadSingleFont(mainFontPath, baseSize, "Khepri UI Default");
    io.FontDefault = FontDefault;

    // 2. Title Font (18px scaled)
    FontTitle = loadSingleFont(mainFontPath, titleSize, "Khepri UI Title");

    // 3. Header Font (16px scaled)
    FontHeader = loadSingleFont(mainFontPath, headerSize, "Khepri UI Header");

    // 4. Small Micro-badge Font (11px scaled)
    FontSmall = loadSingleFont(mainFontPath, smallSize, "Khepri UI Small");

    // 5. Monospace Font (13px scaled)
    if (!monoFontPath.empty() && std::filesystem::exists(monoFontPath)) {
        ImFontConfig monoCfg;
        monoCfg.OversampleH = 3;
        monoCfg.OversampleV = 3;
        FontMono = io.Fonts->AddFontFromFileTTF(monoFontPath.c_str(), monoSize, &monoCfg);
    } else {
        FontMono = FontDefault;
    }

    LOG_INFO("Theme typography loaded successfully from: " + (mainFontPath.empty() ? std::string("default") : mainFontPath));
}

void Theme::PushFontTitle() {
    if (FontTitle && FontTitle->IsLoaded()) ImGui::PushFont(FontTitle);
    else if (FontDefault && FontDefault->IsLoaded()) ImGui::PushFont(FontDefault);
    else ImGui::PushFont(ImGui::GetFont());
}

void Theme::PushFontHeader() {
    if (FontHeader && FontHeader->IsLoaded()) ImGui::PushFont(FontHeader);
    else if (FontDefault && FontDefault->IsLoaded()) ImGui::PushFont(FontDefault);
    else ImGui::PushFont(ImGui::GetFont());
}

void Theme::PushFontSmall() {
    if (FontSmall && FontSmall->IsLoaded()) ImGui::PushFont(FontSmall);
    else if (FontDefault && FontDefault->IsLoaded()) ImGui::PushFont(FontDefault);
    else ImGui::PushFont(ImGui::GetFont());
}

void Theme::PushFontMono() {
    if (FontMono && FontMono->IsLoaded()) ImGui::PushFont(FontMono);
    else if (FontDefault && FontDefault->IsLoaded()) ImGui::PushFont(FontDefault);
    else ImGui::PushFont(ImGui::GetFont());
}

void Theme::PopFont() {
    ImGui::PopFont();
}

} // namespace khepri::ui

#pragma once

#include <imgui.h>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>

namespace khepri::ui {

/**
 * @brief Oryzo / Lusion inspired design system and typography manager for Khepri Engine.
 */
class Theme {
public:
    // Color Constants (RGBA)
    static constexpr ImVec4 COLOR_BG_DARK           = ImVec4(0.063f, 0.071f, 0.086f, 1.0f); // #101216
    static constexpr ImVec4 COLOR_PANEL_BG          = ImVec4(0.082f, 0.094f, 0.114f, 1.0f); // #15181D
    static constexpr ImVec4 COLOR_CARD_BG           = ImVec4(0.106f, 0.122f, 0.149f, 1.0f); // #1B1F26
    static constexpr ImVec4 COLOR_CARD_HOVER        = ImVec4(0.141f, 0.165f, 0.204f, 1.0f); // #242A34
    static constexpr ImVec4 COLOR_CARD_SELECTED     = ImVec4(0.122f, 0.180f, 0.267f, 1.0f); // #1F2E44
    static constexpr ImVec4 COLOR_BORDER            = ImVec4(0.188f, 0.220f, 0.267f, 0.65f); // #303844
    static constexpr ImVec4 COLOR_BORDER_SUBTLE     = ImVec4(0.145f, 0.173f, 0.212f, 0.50f);
    static constexpr ImVec4 COLOR_BORDER_HIGHLIGHT  = ImVec4(0.231f, 0.510f, 0.965f, 0.80f);
    
    // Accents & State Colors
    static constexpr ImVec4 COLOR_ACCENT_PRIMARY    = ImVec4(0.231f, 0.510f, 0.965f, 1.0f); // #3B82F6 (Blue)
    static constexpr ImVec4 COLOR_ACCENT_HOVER      = ImVec4(0.376f, 0.647f, 0.980f, 1.0f); // #60A5FA
    static constexpr ImVec4 COLOR_ACCENT_ACTIVE     = ImVec4(0.145f, 0.388f, 0.922f, 1.0f); // #2563EB
    static constexpr ImVec4 COLOR_ACCENT_CYAN       = ImVec4(0.220f, 0.741f, 0.973f, 1.0f); // #38BDF8
    static constexpr ImVec4 COLOR_SUCCESS           = ImVec4(0.063f, 0.725f, 0.506f, 1.0f); // #10B981 (Emerald)
    static constexpr ImVec4 COLOR_WARNING           = ImVec4(0.961f, 0.620f, 0.043f, 1.0f); // #F59E0B (Amber)
    static constexpr ImVec4 COLOR_ERROR             = ImVec4(0.937f, 0.267f, 0.267f, 1.0f); // #EF4444 (Crimson)
    
    // Text Hierarchy
    static constexpr ImVec4 COLOR_TEXT_PRIMARY      = ImVec4(0.945f, 0.961f, 0.976f, 1.0f); // #F1F5F9
    static constexpr ImVec4 COLOR_TEXT_SECONDARY    = ImVec4(0.580f, 0.639f, 0.722f, 1.0f); // #94A3B8
    static constexpr ImVec4 COLOR_TEXT_MUTED        = ImVec4(0.322f, 0.369f, 0.435f, 1.0f); // #525E6F
    static constexpr ImVec4 COLOR_TEXT_HIGHLIGHT    = ImVec4(0.380f, 0.820f, 1.000f, 1.0f); // Bright blue text

    // Category Badge Colors
    static constexpr ImVec4 COLOR_BADGE_MODEL       = ImVec4(0.188f, 0.450f, 0.910f, 0.25f);
    static constexpr ImVec4 COLOR_BADGE_MODEL_TEXT  = ImVec4(0.450f, 0.720f, 1.000f, 1.0f);
    static constexpr ImVec4 COLOR_BADGE_MATERIAL    = ImVec4(0.850f, 0.450f, 0.120f, 0.25f);
    static constexpr ImVec4 COLOR_BADGE_MATERIAL_TEXT = ImVec4(1.000f, 0.650f, 0.300f, 1.0f);
    static constexpr ImVec4 COLOR_BADGE_TEXTURE     = ImVec4(0.120f, 0.700f, 0.450f, 0.25f);
    static constexpr ImVec4 COLOR_BADGE_TEXTURE_TEXT = ImVec4(0.350f, 0.900f, 0.650f, 1.0f);
    static constexpr ImVec4 COLOR_BADGE_SHADER      = ImVec4(0.650f, 0.250f, 0.850f, 0.25f);
    static constexpr ImVec4 COLOR_BADGE_SHADER_TEXT = ImVec4(0.850f, 0.550f, 1.000f, 1.0f);
    static constexpr ImVec4 COLOR_BADGE_SCENE       = ImVec4(0.850f, 0.750f, 0.150f, 0.25f);
    static constexpr ImVec4 COLOR_BADGE_SCENE_TEXT  = ImVec4(1.000f, 0.900f, 0.350f, 1.0f);

    // Font Atlas Accessors
    static ImFont* FontDefault;
    static ImFont* FontTitle;
    static ImFont* FontHeader;
    static ImFont* FontSmall;
    static ImFont* FontMono;

    /**
     * @brief Configures Dear ImGui style tokens (colors, padding, rounding, spacing)
     * calibrated for the Oryzo/Lusion dark theme with DPI scaling.
     */
    static void ApplyTheme(float scale = 1.0f);

    /**
     * @brief Loads fonts with multiple typographic scales and merges Lucide/Phosphor icons.
     * Searches `assets/fonts/` with fallback to embedded fonts.
     */
    static void LoadFonts(ImGuiIO& io, float scale = 1.0f, const std::string& fontDir = "assets/fonts");

    /**
     * @brief Typography push/pop helpers
     */
    static void PushFontTitle();
    static void PushFontHeader();
    static void PushFontSmall();
    static void PushFontMono();
    static void PopFont();

    // State and Resource Helpers
    static void ResetState();
    static std::filesystem::path FindAssetDirectory(const std::string& subDir = "");

    // Scale State Management
    static float GetContentScale() { return s_contentScale; }
    static void SetContentScale(float scale) { s_contentScale = scale; }
    static float GetUserScale() { return s_userScale; }
    static void SetUserScale(float scale) { s_userScale = scale; }
    static float GetTotalScale() { return s_contentScale * s_userScale; }

private:
    static float s_contentScale;
    static float s_userScale;
};

} // namespace khepri::ui

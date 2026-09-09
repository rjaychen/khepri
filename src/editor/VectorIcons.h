#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include "Theme.h"

namespace khepri::ui {

enum class VectorIconType {
    Mesh,
    Sun,
    PointLight,
    SpotLight,
    Folder,
    FolderOpen,
    File,
    Eye,
    EyeSlash,
    Lock,
    Unlock,
    Camera,
    Material,
    Texture,
    Shader,
    Scene,
    Search,
    Grid,
    List,
    Plus,
    Trash,
    Node
};

/**
 * @brief Procedural vector graphics icons drawn via ImDrawList.
 * Supports DPI scaling and custom tinting.
 */
class VectorIcons {
public:
    static void Draw(ImDrawList* drawList, VectorIconType type, const ImVec2& center, float size, ImU32 color, bool state = false);

    // Individual Vector Icon Draw Routines
    static void DrawMesh(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawSun(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawPointLight(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawSpotLight(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawFolder(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool open = false);
    static void DrawFile(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawEye(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool visible = true);
    static void DrawLock(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool locked = true);
    static void DrawCamera(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawMaterial(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawTexture(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawShader(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawScene(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawSearch(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawGrid(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawList(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawPlus(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawTrash(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);
    static void DrawNode(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color);

    // Dear ImGui widget wrappers
    static void RenderInline(VectorIconType type, float size = 16.0f, const ImVec4& color = Theme::COLOR_TEXT_PRIMARY, bool state = false);
    static bool IconButton(const char* strId, VectorIconType type, bool active = false, const char* tooltip = nullptr, const ImVec2& size = ImVec2(20.0f, 20.0f));
};

} // namespace khepri::ui

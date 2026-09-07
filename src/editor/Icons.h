#pragma once

#include <imgui.h>

namespace khepri::ui {

// Lucide / Phosphor Icon Glyph Definitions (UTF-8)
// These map to standard Unicode icons and PUA glyph codepoints for merged font atlas rendering.
#define ICON_EYE            "\xef\x81\xae" // Eye
#define ICON_EYE_SLASH      "\xef\x81\xb0" // Eye Slash
#define ICON_LOCK           "\xef\x80\xa3" // Lock
#define ICON_UNLOCK         "\xef\x82\x9c" // Unlock
#define ICON_FOLDER         "\xef\x81\xbb" // Folder
#define ICON_FOLDER_OPEN    "\xef\x81\xbc" // Folder Open
#define ICON_FILE           "\xef\x85\x9b" // File
#define ICON_FILE_CODE      "\xef\x87\x89" // File Code
#define ICON_FILE_IMAGE     "\xef\x87\x85" // File Image
#define ICON_FILE_3D        "\xef\x86\xb3" // File 3D / Cube
#define ICON_CUBE           "\xef\x86\xb2" // Cube
#define ICON_SUN            "\xef\x86\x85" // Sun
#define ICON_LIGHTBULB      "\xef\x83\xab" // Lightbulb
#define ICON_SPOTLIGHT      "\xef\x83\x90" // Spotlight
#define ICON_CAMERA         "\xef\x80\xb0" // Camera
#define ICON_SEARCH         "\xef\x80\x82" // Search
#define ICON_GRID           "\xef\x80\x8a" // Grid
#define ICON_LIST           "\xef\x80\x8b" // List
#define ICON_CHEVRON_RIGHT  "\xef\x81\x94" // Chevron Right
#define ICON_CHEVRON_DOWN   "\xef\x81\xb8" // Chevron Down
#define ICON_ARROW_LEFT     "\xef\x81\xa0" // Arrow Left
#define ICON_ARROW_RIGHT    "\xef\x81\xa1" // Arrow Right
#define ICON_ARROW_UP       "\xef\x81\xa2" // Arrow Up
#define ICON_PLUS           "\xef\x80\xa7" // Plus
#define ICON_TRASH          "\xef\x87\xb8" // Trash
#define ICON_EDIT           "\xef\x81\x84" // Edit
#define ICON_RELOAD         "\xef\x80\xa1" // Reload / Refresh
#define ICON_CHECK          "\xef\x80\x8c" // Check
#define ICON_CLOSE          "\xef\x80\x8d" // Close
#define ICON_SETTINGS       "\xef\x80\x93" // Settings
#define ICON_PALETTE        "\xef\x94\xbf" // Palette / Material
#define ICON_LAYERS         "\xef\x97\xbd" // Layers
#define ICON_NODES          "\xef\x8e\x9e" // Graph / Node
#define ICON_MORE_VERT      "\xef\x85\x82" // More Vertical
#define ICON_SHIELD         "\xef\x84\xb2" // Shield
#define ICON_HELP           "\xef\x81\x99" // Help
#define ICON_SORT_ASC       "\xef\x85\x9d" // Sort Ascending
#define ICON_SORT_DESC      "\xef\x85\x9e" // Sort Descending

// Range bounds for font atlas configuration
constexpr ImWchar ICON_MIN_GLYPH = 0xe000;
constexpr ImWchar ICON_MAX_GLYPH = 0xf8ff;

} // namespace khepri::ui

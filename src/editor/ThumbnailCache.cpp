#include "ThumbnailCache.h"
#include "Icons.h"
#include "Theme.h"
#include "../core/Logger.h"
#include <backends/imgui_impl_vulkan.h>
#include <stb_image.h>
#include <algorithm>

namespace khepri::ui {

ThumbnailCache::ThumbnailCache(VulkanContext* context)
    : m_context(context) {
    m_fallbackThumbnail.category = AssetCategory::Unknown;
    m_fallbackThumbnail.badgeText = "FILE";
    m_fallbackThumbnail.vectorIcon = VectorIconType::File;
    m_fallbackThumbnail.badgeBgColor = ImVec4(0.2f, 0.2f, 0.2f, 0.3f);
    m_fallbackThumbnail.badgeTextColor = Theme::COLOR_TEXT_SECONDARY;
}

ThumbnailCache::~ThumbnailCache() {
    Clear();
}

AssetCategory ThumbnailCache::GetCategoryFromPath(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) {
        return AssetCategory::Unknown;
    }

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".stl" || ext == ".fbx" || ext == ".dae") {
        return AssetCategory::Model3D;
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
        return AssetCategory::Texture2D;
    } else if (ext == ".mat" || ext == ".material") {
        return AssetCategory::Material;
    } else if (ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".spv" || ext == ".glsl") {
        return AssetCategory::Shader;
    } else if (ext == ".khepri" || ext == ".scene" || ext == ".json") {
        return AssetCategory::Scene;
    } else if (ext == ".txt" || ext == ".md" || ext == ".log") {
        return AssetCategory::Document;
    }

    return AssetCategory::Unknown;
}

const char* ThumbnailCache::GetCategoryName(AssetCategory category) {
    switch (category) {
        case AssetCategory::All:       return "All Assets";
        case AssetCategory::Model3D:   return "3D Models";
        case AssetCategory::Texture2D: return "Textures";
        case AssetCategory::Material:  return "Materials";
        case AssetCategory::Shader:    return "Shaders";
        case AssetCategory::Scene:     return "Scenes";
        case AssetCategory::Document:  return "Documents";
        default:                       return "Other";
    }
}

const AssetThumbnail& ThumbnailCache::GetOrCreateThumbnail(const std::filesystem::path& assetPath) {
    std::string key = assetPath.string();
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.lastAccessTime = std::chrono::steady_clock::now();
        return it->second;
    }

    AssetThumbnail thumb{};
    thumb.category = GetCategoryFromPath(assetPath);
    thumb.lastAccessTime = std::chrono::steady_clock::now();

    std::string ext = assetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    thumb.badgeText = ext.empty() ? "FILE" : ext;

    // Configure Badge Colors & Vector Icons by category
    switch (thumb.category) {
        case AssetCategory::Model3D:
            thumb.vectorIcon = VectorIconType::Mesh;
            thumb.badgeBgColor = Theme::COLOR_BADGE_MODEL;
            thumb.badgeTextColor = Theme::COLOR_BADGE_MODEL_TEXT;
            break;
        case AssetCategory::Texture2D:
            thumb.vectorIcon = VectorIconType::Texture;
            thumb.badgeBgColor = Theme::COLOR_BADGE_TEXTURE;
            thumb.badgeTextColor = Theme::COLOR_BADGE_TEXTURE_TEXT;
            break;
        case AssetCategory::Material:
            thumb.vectorIcon = VectorIconType::Material;
            thumb.badgeBgColor = Theme::COLOR_BADGE_MATERIAL;
            thumb.badgeTextColor = Theme::COLOR_BADGE_MATERIAL_TEXT;
            break;
        case AssetCategory::Shader:
            thumb.vectorIcon = VectorIconType::Shader;
            thumb.badgeBgColor = Theme::COLOR_BADGE_SHADER;
            thumb.badgeTextColor = Theme::COLOR_BADGE_SHADER_TEXT;
            break;
        case AssetCategory::Scene:
            thumb.vectorIcon = VectorIconType::Scene;
            thumb.badgeBgColor = Theme::COLOR_BADGE_SCENE;
            thumb.badgeTextColor = Theme::COLOR_BADGE_SCENE_TEXT;
            break;
        case AssetCategory::Document:
            thumb.vectorIcon = VectorIconType::File;
            thumb.badgeBgColor = ImVec4(0.2f, 0.2f, 0.2f, 0.3f);
            thumb.badgeTextColor = Theme::COLOR_TEXT_SECONDARY;
            break;
        default:
            thumb.vectorIcon = std::filesystem::is_directory(assetPath) ? VectorIconType::Folder : VectorIconType::File;
            thumb.badgeBgColor = ImVec4(0.2f, 0.2f, 0.2f, 0.3f);
            thumb.badgeTextColor = Theme::COLOR_TEXT_MUTED;
            break;
    }

    // Attempt to load GPU preview texture if it is an image file
    if (thumb.category == AssetCategory::Texture2D && m_context && std::filesystem::exists(assetPath)) {
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(assetPath.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (pixels) {
            thumb.width = static_cast<uint32_t>(w);
            thumb.height = static_cast<uint32_t>(h);
            stbi_image_free(pixels);
        }
    }

    auto inserted = m_cache.emplace(key, std::move(thumb));
    return inserted.first->second;
}

void ThumbnailCache::EvictLRU(size_t maxCapacity) {
    if (m_cache.size() <= maxCapacity) return;

    // Find oldest access time entries
    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
    for (const auto& [k, v] : m_cache) {
        entries.emplace_back(k, v.lastAccessTime);
    }

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    size_t toRemove = m_cache.size() - maxCapacity;
    for (size_t i = 0; i < toRemove && i < entries.size(); ++i) {
        auto it = m_cache.find(entries[i].first);
        if (it != m_cache.end()) {
            if (it->second.descriptorSet != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(it->second.descriptorSet);
                it->second.descriptorSet = VK_NULL_HANDLE;
            }
            m_cache.erase(it);
        }
    }
}

void ThumbnailCache::Clear() {
    for (auto& [k, v] : m_cache) {
        if (v.descriptorSet != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(v.descriptorSet);
            v.descriptorSet = VK_NULL_HANDLE;
        }
    }
    m_cache.clear();
}

} // namespace khepri::ui

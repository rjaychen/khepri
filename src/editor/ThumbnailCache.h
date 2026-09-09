#pragma once

#include <imgui.h>
#include <volk.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Texture.h"

#include "VectorIcons.h"

namespace khepri::ui {

enum class AssetCategory {
    All,
    Model3D,
    Texture2D,
    Material,
    Shader,
    Scene,
    Document,
    Unknown
};

struct AssetThumbnail {
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    std::shared_ptr<Texture> texture;
    AssetCategory category{AssetCategory::Unknown};
    std::string badgeText;
    ImVec4 badgeBgColor{0.2f, 0.2f, 0.2f, 0.3f};
    ImVec4 badgeTextColor{0.8f, 0.8f, 0.8f, 1.0f};
    VectorIconType vectorIcon{VectorIconType::File};
    uint32_t width{0};
    uint32_t height{0};
    std::chrono::steady_clock::time_point lastAccessTime;
};

/**
 * @brief Dedicated Vulkan descriptor set and thumbnail manager for asset previews.
 */
class ThumbnailCache {
public:
    ThumbnailCache(VulkanContext* context = nullptr);
    ~ThumbnailCache();

    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    void SetContext(VulkanContext* context) { m_context = context; }

    /**
     * @brief Gets or creates a preview metadata / descriptor set for the given asset path.
     */
    const AssetThumbnail& GetOrCreateThumbnail(const std::filesystem::path& assetPath);

    /**
     * @brief Returns the determined asset category from extension.
     */
    static AssetCategory GetCategoryFromPath(const std::filesystem::path& path);

    /**
     * @brief Category name string for UI filters.
     */
    static const char* GetCategoryName(AssetCategory category);

    /**
     * @brief Evicts old cached entries exceeding maxCapacity.
     */
    void EvictLRU(size_t maxCapacity = 128);

    /**
     * @brief Clears all cached thumbnails and releases all Vulkan descriptor sets.
     */
    void Clear();

private:
    VulkanContext* m_context{nullptr};
    std::unordered_map<std::string, AssetThumbnail> m_cache;
    AssetThumbnail m_fallbackThumbnail;
};

} // namespace khepri::ui

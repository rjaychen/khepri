#pragma once

#include "../material/Material.h"
#include "../vulkan/Texture.h"
#include "../vulkan/VulkanContext.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace khepri {

class AssetManager {
public:
    static AssetManager& Instance();

    void Initialize(VulkanContext& context);

    std::shared_ptr<Material> CreateMaterial(const std::string& name);
    std::shared_ptr<Material> GetMaterial(const std::string& name);

    std::shared_ptr<Material> GetDefaultMaterial() const { return m_defaultMaterial; }
    std::shared_ptr<Material> GetDefaultEmissiveLightMaterial() const { return m_defaultEmissiveLightMaterial; }
    const std::unordered_map<std::string, std::shared_ptr<Material>>& GetMaterials() const { return m_materials; }

private:
    AssetManager() = default;
    ~AssetManager() = default;

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    VulkanContext* m_context = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materials;
    std::shared_ptr<Material> m_defaultMaterial;
    std::shared_ptr<Material> m_defaultEmissiveLightMaterial;
};

} // namespace khepri

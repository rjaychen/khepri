#include "AssetManager.h"
#include "../core/Logger.h"

namespace khepri {

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

void AssetManager::Initialize(VulkanContext& context) {
    m_context = &context;
    
    // Create default fallback materials
    m_defaultMaterial = std::make_shared<Material>("DefaultPBR");
    m_defaultMaterial->SetBaseColorFactor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

    m_defaultEmissiveLightMaterial = std::make_shared<Material>("EmissiveLightGizmo");
    m_defaultEmissiveLightMaterial->SetBaseColorFactor(glm::vec4(1.0f, 0.95f, 0.8f, 1.0f));
    m_defaultEmissiveLightMaterial->SetEmissiveFactor(glm::vec4(1.0f, 0.95f, 0.8f, 5.0f));

    m_materials["DefaultPBR"] = m_defaultMaterial;
    m_materials["EmissiveLightGizmo"] = m_defaultEmissiveLightMaterial;

    LOG_INFO("AssetManager initialized with default materials.");
}

std::shared_ptr<Material> AssetManager::CreateMaterial(const std::string& name) {
    auto mat = std::make_shared<Material>(name);
    m_materials[name] = mat;
    return mat;
}

std::shared_ptr<Material> AssetManager::GetMaterial(const std::string& name) {
    auto it = m_materials.find(name);
    if (it != m_materials.end()) return it->second;
    return m_defaultMaterial;
}

} // namespace khepri

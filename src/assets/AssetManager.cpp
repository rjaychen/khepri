#include "AssetManager.h"
#include "../core/Logger.h"
#include <fstream>
#include <iomanip>

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

    m_defaultEmissiveLightMaterial = std::make_shared<Material>("EmissiveLight");
    m_defaultEmissiveLightMaterial->SetBaseColorFactor(glm::vec4(1.0f, 0.95f, 0.8f, 1.0f));
    m_defaultEmissiveLightMaterial->SetEmissiveFactor(glm::vec4(1.0f, 0.95f, 0.8f, 5.0f));

    m_materials["DefaultPBR"] = m_defaultMaterial;
    m_materials["EmissiveLight"] = m_defaultEmissiveLightMaterial;

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

std::shared_ptr<Material> AssetManager::LoadMaterial(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        LOG_WARN("Material file does not exist: " + path.string());
        return nullptr;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open material file: " + path.string());
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string name = path.stem().string();
    auto mat = std::make_shared<Material>(name);

    auto extractFloats = [](const std::string& str, const std::string& key) -> std::vector<float> {
        std::vector<float> result;
        size_t pos = str.find("\"" + key + "\"");
        if (pos == std::string::npos) return result;
        size_t start = str.find('[', pos);
        size_t end = str.find(']', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string sub = str.substr(start + 1, end - start - 1);
            std::stringstream ss(sub);
            std::string item;
            while (std::getline(ss, item, ',')) {
                try {
                    result.push_back(std::stof(item));
                } catch (...) {}
            }
        }
        return result;
    };

    auto extractFloat = [](const std::string& str, const std::string& key, float defaultVal) -> float {
        size_t pos = str.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        size_t colon = str.find(':', pos);
        if (colon == std::string::npos) return defaultVal;
        size_t end = str.find_first_of(",}\n", colon + 1);
        if (end == std::string::npos) end = str.length();
        try {
            return std::stof(str.substr(colon + 1, end - colon - 1));
        } catch (...) {
            return defaultVal;
        }
    };

    auto baseColor = extractFloats(content, "baseColor");
    if (baseColor.size() == 4) {
        mat->SetBaseColorFactor(glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]));
    } else if (baseColor.size() == 3) {
        mat->SetBaseColorFactor(glm::vec4(baseColor[0], baseColor[1], baseColor[2], 1.0f));
    }

    auto emissive = extractFloats(content, "emissive");
    if (emissive.size() == 4) {
        mat->SetEmissiveFactor(glm::vec4(emissive[0], emissive[1], emissive[2], emissive[3]));
    } else if (emissive.size() == 3) {
        mat->SetEmissiveFactor(glm::vec4(emissive[0], emissive[1], emissive[2], 1.0f));
    }

    mat->SetRoughness(extractFloat(content, "roughness", 0.5f));
    mat->SetMetallic(extractFloat(content, "metallic", 0.0f));

    m_materials[name] = mat;
    LOG_INFO("Loaded material: " + name + " from " + path.string());
    return mat;
}

bool AssetManager::SaveMaterial(const Material& material, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to write material file: " + path.string());
        return false;
    }

    const glm::vec4& color = material.GetBaseColorFactor();
    const glm::vec4& emissive = material.GetEmissiveFactor();

    file << std::fixed << std::setprecision(3);
    file << "{\n";
    file << "  \"name\": \"" << material.GetName() << "\",\n";
    file << "  \"baseColor\": [" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << "],\n";
    file << "  \"roughness\": " << material.GetRoughness() << ",\n";
    file << "  \"metallic\": " << material.GetMetallic() << ",\n";
    file << "  \"emissive\": [" << emissive.r << ", " << emissive.g << ", " << emissive.b << ", " << emissive.a << "]\n";
    file << "}\n";

    LOG_INFO("Saved material: " + material.GetName() + " to " + path.string());
    return true;
}

} // namespace khepri

#include "AssetManagerPanel.h"
#include "../core/Logger.h"
#include <glm/gtc/type_ptr.hpp>

namespace khepri {

void AssetManagerPanel::RenderUI(bool* p_open) {
    if (p_open && !*p_open) return;

    if (!ImGui::Begin("Asset Manager", p_open)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Asset Registry & Material Library");
    ImGui::Separator();

    // Section 1: Create New Material
    ImGui::Text("Create Material:");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("##NewMatName", m_newMaterialName, sizeof(m_newMaterialName));
    ImGui::SameLine();
    if (ImGui::Button("+ Create Material")) {
        if (strlen(m_newMaterialName) > 0) {
            AssetManager::Instance().CreateMaterial(m_newMaterialName);
            m_selectedMaterialName = m_newMaterialName;
            LOG_INFO("Created new material: " + std::string(m_newMaterialName));
        }
    }

    ImGui::Separator();

    // Section 2: Material Explorer
    ImGui::Columns(2, "AssetManagerColumns", true);
    ImGui::SetColumnWidth(0, 200.0f);

    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Materials (%zu)", AssetManager::Instance().GetMaterials().size());
    ImGui::Separator();

    for (const auto& [name, mat] : AssetManager::Instance().GetMaterials()) {
        bool isSelected = (name == m_selectedMaterialName);
        if (ImGui::Selectable(name.c_str(), isSelected)) {
            m_selectedMaterialName = name;
        }
    }

    ImGui::NextColumn();

    // Section 3: Material Inspector
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Material Properties:");
    ImGui::Separator();

    auto selectedMat = AssetManager::Instance().GetMaterial(m_selectedMaterialName);
    if (selectedMat) {
        ImGui::Text("Name: %s", selectedMat->GetName().c_str());
        
        glm::vec4 color = selectedMat->GetBaseColorFactor();
        if (ImGui::ColorEdit4("Base Color Factor", glm::value_ptr(color))) {
            selectedMat->SetBaseColorFactor(color);
        }

        glm::vec4 emissive = selectedMat->GetEmissiveFactor();
        if (ImGui::ColorEdit4("Emissive Factor", glm::value_ptr(emissive))) {
            selectedMat->SetEmissiveFactor(emissive);
        }

        float roughness = selectedMat->GetRoughness();
        if (ImGui::DragFloat("Roughness", &roughness, 0.01f, 0.0f, 1.0f)) {
            selectedMat->SetRoughness(roughness);
        }

        float metallic = selectedMat->GetMetallic();
        if (ImGui::DragFloat("Metallic", &metallic, 0.01f, 0.0f, 1.0f)) {
            selectedMat->SetMetallic(metallic);
        }

        ImGui::Text("Albedo Texture: %s", selectedMat->GetAlbedoTexture() ? "Loaded" : "Default White");
    } else {
        ImGui::TextDisabled("Select a material to edit properties.");
    }

    ImGui::Columns(1);
    ImGui::End();
}

} // namespace khepri

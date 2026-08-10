#pragma once

#include "../assets/AssetManager.h"
#include <imgui.h>
#include <memory>
#include <string>

namespace khepri {

class AssetManagerPanel {
public:
    AssetManagerPanel() = default;
    ~AssetManagerPanel() = default;

    void RenderUI(bool* p_open = nullptr);

private:
    char m_newMaterialName[128] = "CustomMaterial";
    std::string m_selectedMaterialName = "DefaultPBR";
};

} // namespace khepri

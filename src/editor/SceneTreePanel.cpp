#include "SceneTreePanel.h"
#include "../scene/MeshComponent.h"
#include <glm/gtc/type_ptr.hpp>

SceneTreePanel::SceneTreePanel(VulkanContext& context)
    : m_context(context) {}

void SceneTreePanel::RenderUI(SceneNode* rootNode) {
    ImGui::Begin("Scene Hierarchy");

    if (ImGui::Button("+ Add Empty Node")) {
        auto node = std::make_unique<SceneNode>("New Node");
        rootNode->AddChild(std::move(node));
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add Cube")) {
        auto node = std::make_unique<SceneNode>("Cube Node");
        rootNode->AddChild(std::move(node));
    }

    ImGui::Separator();
    if (rootNode) {
        RenderNodeTree(rootNode);
    }

    ImGui::End();

    ImGui::Begin("Inspector");
    if (m_selectedNode) {
        RenderInspector(m_selectedNode);
    } else {
        ImGui::TextDisabled("No Node Selected");
    }
    ImGui::End();
}

void SceneTreePanel::RenderNodeTree(SceneNode* node) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (node == m_selectedNode) flags |= ImGuiTreeNodeFlags_Selected;
    if (node->GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx((void*)(intptr_t)node->id, flags, "%s", node->name.c_str());
    if (ImGui::IsItemClicked()) {
        m_selectedNode = node;
    }

    if (opened) {
        for (const auto& child : node->GetChildren()) {
            RenderNodeTree(child.get());
        }
        ImGui::TreePop();
    }
}

void SceneTreePanel::RenderInspector(SceneNode* node) {
    char nameBuf[256];
    strncpy(nameBuf, node->name.c_str(), sizeof(nameBuf));
    if (ImGui::InputText("Node Name", nameBuf, sizeof(nameBuf))) {
        node->name = nameBuf;
    }

    ImGui::Separator();
    ImGui::Text("Transform Properties");

    for (auto& prop : node->GetProperties()) {
        if (prop.type == PropertyType::Vec3) {
            glm::vec3 val = prop.GetValue<glm::vec3>();
            if (ImGui::DragFloat3(prop.name.c_str(), glm::value_ptr(val), 0.1f, prop.minVal, prop.maxVal)) {
                prop.SetValue(val);
            }
        }
    }
}

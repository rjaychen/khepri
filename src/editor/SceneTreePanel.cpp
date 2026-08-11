#include "SceneTreePanel.h"
#include "../scene/MeshComponent.h"
#include <glm/gtc/type_ptr.hpp>

SceneTreePanel::SceneTreePanel(VulkanContext& context)
    : m_context(context) {}

#include "../assets/AssetManager.h"
#include <fstream>

void SceneTreePanel::RenderUI(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh,
                              const std::filesystem::path& selectedAssetPath) {
    ImGui::Begin("Scene Hierarchy");

    // --- Add Dropdown Section ---
    if (ImGui::Button("+ Add Node")) {
        ImGui::OpenPopup("AddNodeDropdownPopup");
    }

    if (ImGui::BeginPopup("AddNodeDropdownPopup")) {
        if (rootNode) {
            if (ImGui::BeginMenu("Primitive")) {
                if (ImGui::MenuItem("Cube")) {
                    auto newMesh = MeshComponent::CreateCube(m_context, 1.0f);
                    auto newNode = std::make_unique<SceneNode>("Cube");
                    newNode->mesh = newMesh;
                    m_selectedNode = rootNode->AddChild(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Sphere")) {
                    auto newMesh = MeshComponent::CreateSphere(m_context, 0.6f, 32, 16);
                    auto newNode = std::make_unique<SceneNode>("Sphere");
                    newNode->mesh = newMesh;
                    m_selectedNode = rootNode->AddChild(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Cylinder")) {
                    auto newMesh = MeshComponent::CreateCylinder(m_context, 0.4f, 1.0f, 32);
                    auto newNode = std::make_unique<SceneNode>("Cylinder");
                    newNode->mesh = newMesh;
                    m_selectedNode = rootNode->AddChild(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Plane")) {
                    auto newMesh = MeshComponent::CreatePlane(m_context, 4.0f, 8);
                    auto newNode = std::make_unique<SceneNode>("Plane");
                    newNode->mesh = newMesh;
                    m_selectedNode = rootNode->AddChild(std::move(newNode));
                    activeMesh = newMesh;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Directional Light (Sun)")) {
                    auto node = std::make_unique<SceneNode>("Directional Light");
                    node->position = glm::vec3(0.0f, 10.0f, 0.0f);
                    node->rotationDegrees = glm::vec3(-45.0f, 45.0f, 0.0f);
                    node->lightComponent = std::make_shared<LightComponent>(LightType::Directional);
                    node->lightComponent->color = glm::vec3(1.0f, 0.95f, 0.85f);
                    m_selectedNode = rootNode->AddChild(std::move(node));
                }
                if (ImGui::MenuItem("Point Light")) {
                    auto node = std::make_unique<SceneNode>("Point Light");
                    node->position = glm::vec3(0.0f, 3.0f, 0.0f);
                    node->lightComponent = std::make_shared<LightComponent>(LightType::Point);
                    node->lightComponent->color = glm::vec3(1.0f, 0.8f, 0.4f);
                    node->lightComponent->intensity = 2.0f;
                    m_selectedNode = rootNode->AddChild(std::move(node));
                }
                if (ImGui::MenuItem("Spot Light")) {
                    auto node = std::make_unique<SceneNode>("Spot Light");
                    node->position = glm::vec3(0.0f, 4.0f, 2.0f);
                    node->rotationDegrees = glm::vec3(-30.0f, 0.0f, 0.0f);
                    node->lightComponent = std::make_shared<LightComponent>(LightType::Spot);
                    node->lightComponent->color = glm::vec3(0.4f, 0.8f, 1.0f);
                    node->lightComponent->intensity = 3.0f;
                    m_selectedNode = rootNode->AddChild(std::move(node));
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Empty Node")) {
                auto node = std::make_unique<SceneNode>("Empty Node");
                m_selectedNode = rootNode->AddChild(std::move(node));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // --- Scene Tree ---
    if (rootNode) {
        RenderNodeTree(rootNode);
    }
    if (rootNode && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE_PTR")) {
            SceneNode* draggedNode = *(SceneNode**)payload->Data;
            if (draggedNode && draggedNode->GetParent() != rootNode && draggedNode != rootNode) {
                SceneNode* oldParent = draggedNode->GetParent();
                if (oldParent) {
                    auto detached = oldParent->DetachChild(draggedNode);
                    if (detached) {
                        rootNode->AddChild(std::move(detached));
                    }
                }
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH_PAYLOAD")) {
            const char* pathStr = static_cast<const char*>(payload->Data);
            if (m_onImportModel) {
                m_onImportModel(pathStr);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

    // --- Inspector Panel ---
    ImGui::Begin("Inspector");
    if (m_selectedNode) {
        RenderInspector(m_selectedNode, selectedAssetPath);
    } else if (!selectedAssetPath.empty()) {
        RenderFileAssetInspector(selectedAssetPath);
    } else {
        ImGui::TextDisabled("No Node or Asset File Selected");
    }
    ImGui::End();
}

void SceneTreePanel::RenderFileAssetInspector(const std::filesystem::path& assetPath) {
    if (!std::filesystem::exists(assetPath)) {
        ImGui::TextDisabled("Selected file no longer exists.");
        return;
    }

    std::string filename = assetPath.filename().string();
    std::string ext = assetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Asset File Inspector");
    ImGui::Separator();

    ImGui::Text("File Name: %s", filename.c_str());
    ImGui::Text("Path: %s", assetPath.string().c_str());

    uintmax_t sizeBytes = 0;
    try {
        sizeBytes = std::filesystem::file_size(assetPath);
    } catch (...) {}

    if (sizeBytes < 1024) {
        ImGui::Text("Size: %zu Bytes", sizeBytes);
    } else if (sizeBytes < 1024 * 1024) {
        ImGui::Text("Size: %.2f KB", sizeBytes / 1024.0f);
    } else {
        ImGui::Text("Size: %.2f MB", sizeBytes / (1024.0f * 1024.0f));
    }

    // Default asset check
    std::string lowerPath = assetPath.string();
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowerPath.find("bunny") != std::string::npos || lowerPath.find("duck") != std::string::npos ||
        lowerPath.find("teapot") != std::string::npos || lowerPath.find("box.gltf") != std::string::npos) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "🌟 Default Engine Asset Model");
    }

    ImGui::Separator();

    bool isModel = (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".stl");
    if (isModel) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "3D Geometry Model Asset");
        ImGui::Spacing();
        
        if (ImGui::Button("Open Scene with Model (Replace Current)", ImVec2(-1, 30))) {
            if (m_onOpenModel) m_onOpenModel(assetPath.string());
        }
        if (ImGui::Button("Import Model into Current Scene", ImVec2(-1, 30))) {
            if (m_onImportModel) m_onImportModel(assetPath.string());
        }
    } else if (ext == ".mat") {
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Material Preset Asset");
        std::string matName = assetPath.stem().string();
        auto mat = khepri::AssetManager::Instance().GetMaterial(matName);
        if (mat) {
            glm::vec4 color = mat->GetBaseColorFactor();
            if (ImGui::ColorEdit4("Base Color Factor", glm::value_ptr(color))) {
                mat->SetBaseColorFactor(color);
            }
            float roughness = mat->GetRoughness();
            if (ImGui::DragFloat("Roughness", &roughness, 0.01f, 0.0f, 1.0f)) {
                mat->SetRoughness(roughness);
            }
            float metallic = mat->GetMetallic();
            if (ImGui::DragFloat("Metallic", &metallic, 0.01f, 0.0f, 1.0f)) {
                mat->SetMetallic(metallic);
            }
        }
    } else if (ext == ".txt" || ext == ".json") {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Text Document Asset");
        ImGui::Separator();
        std::ifstream file(assetPath);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            if (content.size() > 1000) content = content.substr(0, 1000) + "... (truncated)";
            ImGui::TextWrapped("%s", content.c_str());
        }
    }
}


void SceneTreePanel::RenderNodeTree(SceneNode* node) {
    if (!node) return;

    ImGui::PushID(static_cast<int>(node->id));

    // --- Visibility Checkbox (eye toggle) ---
    bool vis = node->visible;
    if (ImGui::Checkbox("##vis", &vis)) {
        node->visible = vis;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vis ? "Visible — click to hide" : "Hidden — click to show");
    }
    ImGui::SameLine();

    // --- Tree Node Label (greyed out if hidden) ---
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_OpenOnDoubleClick
                             | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node == m_selectedNode)      flags |= ImGuiTreeNodeFlags_Selected;
    if (node->GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    std::string nodeLabel = node->name;
    bool isLight = (node->lightComponent != nullptr);
    if (isLight) {
        switch (node->lightComponent->type) {
            case LightType::Directional: nodeLabel = "[Dir Light] " + node->name; break;
            case LightType::Point:       nodeLabel = "[Point Light] " + node->name; break;
            case LightType::Spot:        nodeLabel = "[Spot Light] " + node->name; break;
        }
    }

    int colorsPushed = 0;
    if (!node->visible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 0.7f));
        colorsPushed++;
    } else if (isLight) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.30f, 1.0f)); // Bright yellow indicator for lights
        colorsPushed++;
    }

    bool opened = ImGui::TreeNodeEx("##node", flags, "%s", nodeLabel.c_str());

    if (colorsPushed > 0) ImGui::PopStyleColor(colorsPushed);

    // --- Drag and Drop Source (Drag node to move) ---
    if (node->GetParent() != nullptr && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SCENE_NODE_PTR", &node, sizeof(SceneNode*));
        ImGui::Text("Move node '%s'", node->name.c_str());
        ImGui::EndDragDropSource();
    }

    // --- Drag and Drop Target (Drop node onto target parent) ---
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE_PTR")) {
            SceneNode* draggedNode = *(SceneNode**)payload->Data;
            if (draggedNode && draggedNode != node && draggedNode->GetParent() != node && !node->IsDescendantOf(draggedNode)) {
                SceneNode* oldParent = draggedNode->GetParent();
                if (oldParent) {
                    auto detached = oldParent->DetachChild(draggedNode);
                    if (detached) {
                        node->AddChild(std::move(detached));
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        m_selectedNode = node;
    }

    // Context menu on right-click
    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (ImGui::MenuItem("Delete")) {
            if (node->GetParent()) {
                if (m_selectedNode == node) m_selectedNode = nullptr;
                auto nodeHasMesh = [](SceneNode* n, auto& recurse) -> bool {
                    if (n->mesh) return true;
                    for (const auto& c : n->GetChildren())
                        if (recurse(c.get(), recurse)) return true;
                    return false;
                };
                if (nodeHasMesh(node, nodeHasMesh)) {
                    m_context.WaitIdle();
                }
                node->GetParent()->RemoveChild(node);
                ImGui::EndPopup();
                if (opened) ImGui::TreePop();
                ImGui::PopID();
                return;
            }
        }
        ImGui::EndPopup();
    }

    if (opened) {
        for (size_t i = 0; i < node->GetChildren().size(); ++i) {
            RenderNodeTree(node->GetChildren()[i].get());
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneTreePanel::RenderInspector(SceneNode* node, const std::filesystem::path& selectedAssetPath) {
    (void)selectedAssetPath;
    char nameBuf[256];
    strncpy(nameBuf, node->name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Node Name", nameBuf, sizeof(nameBuf))) {
        node->name = nameBuf;
    }

    // Visibility toggle in inspector too
    ImGui::Checkbox("Visible", &node->visible);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Transform");

    // Position
    if (ImGui::DragFloat3("Position", glm::value_ptr(node->position), 0.1f, -100.0f, 100.0f)) {
        node->SyncPropertiesToTransform();
    }

    // Rotation
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(node->rotationDegrees), 0.1f, -360.0f, 360.0f)) {
        node->SyncPropertiesToTransform();
    }

    // Scale Lock & Scale
    ImGui::Checkbox("🔒 Lock Scale Aspect Ratio", &node->lockScale);
    glm::vec3 oldScale = node->scale;
    glm::vec3 newScale = oldScale;
    if (ImGui::DragFloat3("Scale", glm::value_ptr(newScale), 0.01f, 0.001f, 100.0f)) {
        if (node->lockScale) {
            float factor = 1.0f;
            if (std::abs(newScale.x - oldScale.x) > 1e-5f && oldScale.x > 1e-5f) {
                factor = newScale.x / oldScale.x;
            } else if (std::abs(newScale.y - oldScale.y) > 1e-5f && oldScale.y > 1e-5f) {
                factor = newScale.y / oldScale.y;
            } else if (std::abs(newScale.z - oldScale.z) > 1e-5f && oldScale.z > 1e-5f) {
                factor = newScale.z / oldScale.z;
            }
            newScale = glm::clamp(oldScale * factor, glm::vec3(0.001f), glm::vec3(100.0f));
        }
        node->scale = newScale;
        node->SyncPropertiesToTransform();
    }

    if (node->lightComponent) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "💡 Light Component");

        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(node->lightComponent->type);
        if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
            node->lightComponent->type = static_cast<LightType>(currentType);
        }

        ImGui::ColorEdit3("Light Color", glm::value_ptr(node->lightComponent->color));
        ImGui::DragFloat("Intensity", &node->lightComponent->intensity, 0.05f, 0.0f, 100.0f);

        if (node->lightComponent->type == LightType::Directional || node->lightComponent->type == LightType::Spot) {
            ImGui::DragFloat3("Local Direction", glm::value_ptr(node->lightComponent->direction), 0.02f, -1.0f, 1.0f);
        }

        if (node->lightComponent->type == LightType::Point || node->lightComponent->type == LightType::Spot) {
            ImGui::TextDisabled("Distance Attenuation Factors:");
            ImGui::DragFloat("Constant", &node->lightComponent->constantAttenuation, 0.01f, 0.1f, 10.0f);
            ImGui::DragFloat("Linear", &node->lightComponent->linearAttenuation, 0.005f, 0.0f, 2.0f);
            ImGui::DragFloat("Quadratic", &node->lightComponent->quadraticAttenuation, 0.001f, 0.0f, 1.0f);
        }

        if (node->lightComponent->type == LightType::Spot) {
            ImGui::TextDisabled("Spot Cone Angles:");
            ImGui::DragFloat("Inner Cutoff (°)", &node->lightComponent->innerCutoffAngle, 0.5f, 0.0f, 80.0f);
            ImGui::DragFloat("Outer Cutoff (°)", &node->lightComponent->outerCutoffAngle, 0.5f, node->lightComponent->innerCutoffAngle, 89.0f);
        }
    }

    if (node->mesh) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Mesh Material & Topology");
        ImGui::BulletText("Vertices: %zu",   node->mesh->GetVertices().size());
        ImGui::BulletText("Triangles: %zu",  node->mesh->GetIndices().size() / 3);
        ImGui::BulletText("Has Texture: %s", node->mesh->HasTexture() ? "Yes (Loaded)" : "No (Default White)");

        const char* wireframeOptions[] = { "Solid", "Wireframe Overlay", "Wireframe Only" };
        int currentWire = static_cast<int>(node->wireframeMode);
        if (ImGui::Combo("Wireframe Display", &currentWire, wireframeOptions, IM_ARRAYSIZE(wireframeOptions))) {
            node->wireframeMode = static_cast<WireframeMode>(currentWire);
        }

        glm::vec4 colorFactor = node->mesh->GetBaseColorFactor();
        if (ImGui::ColorEdit4("Base Color Factor", glm::value_ptr(colorFactor))) {
            node->mesh->SetBaseColorFactor(colorFactor);
        }
    }
}

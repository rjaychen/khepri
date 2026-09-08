#include "SceneTreePanel.h"
#include "NodeGraphEditorPanel.h"
#include "VectorIcons.h"
#include "Theme.h"
#include "UIWidgets.h"
#include "../scene/MeshComponent.h"
#include "../scene/LightNode.h"
#include "../scene/SceneHierarchyCommand.h"
#include "../scene/TransformCommand.h"
#include "../core/UndoStack.h"
#include "../assets/AssetManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui_internal.h>
#include <fstream>
#include <algorithm>

using namespace khepri::ui;

SceneTreePanel::SceneTreePanel(VulkanContext* context)
    : m_context(context) {}

SceneTreePanel::SceneTreePanel(VulkanContext& context)
    : SceneTreePanel(&context) {}

void SceneTreePanel::SetSelectedNode(SceneNode* node) {
    m_selectedNode = node;
    m_selectedNodes.clear();
    if (node) {
        m_selectedNodes.insert(node);
        m_lastSelectedNode = node;
    }
}

void SceneTreePanel::ClearSelectedNode() {
    m_selectedNode = nullptr;
    m_selectedNodes.clear();
    m_lastSelectedNode = nullptr;
}

void SceneTreePanel::SelectNode(SceneNode* node, bool additive) {
    if (!node) return;
    if (!additive) {
        m_selectedNodes.clear();
    }
    m_selectedNodes.insert(node);
    m_selectedNode = node;
    m_lastSelectedNode = node;
}

void SceneTreePanel::DeselectNode(SceneNode* node) {
    if (!node) return;
    m_selectedNodes.erase(node);
    if (m_selectedNode == node) {
        m_selectedNode = m_selectedNodes.empty() ? nullptr : *m_selectedNodes.begin();
    }
}

void SceneTreePanel::SelectAll(SceneNode* rootNode) {
    if (!rootNode) return;
    m_selectedNodes.clear();
    std::vector<SceneNode*> flattened;
    for (const auto& child : rootNode->GetChildren()) {
        CollectFlattenedNodes(child.get(), flattened);
    }
    for (auto* n : flattened) {
        m_selectedNodes.insert(n);
    }
    m_selectedNode = flattened.empty() ? nullptr : flattened.front();
}

void SceneTreePanel::ClearAllSelections() {
    ClearSelectedNode();
}

void SceneTreePanel::ValidateSelection(const SceneNode* rootNode) noexcept {
    if (!rootNode) {
        m_selectedNode = nullptr;
        m_selectedNodes.clear();
        m_lastSelectedNode = nullptr;
        return;
    }

    if (m_selectedNode && !rootNode->Contains(m_selectedNode)) {
        m_selectedNode = nullptr;
    }

    // Clean up any deleted nodes from multi-selection set
    for (auto it = m_selectedNodes.begin(); it != m_selectedNodes.end();) {
        if (!(*it) || !rootNode->Contains(*it)) {
            it = m_selectedNodes.erase(it);
        } else {
            ++it;
        }
    }

    if (m_selectedNode == nullptr && !m_selectedNodes.empty()) {
        m_selectedNode = *m_selectedNodes.begin();
    }
}

void SceneTreePanel::StartRenaming(SceneNode* node) {
    if (!node) return;
    m_renamingNode = node;
    strncpy(m_renameBuffer, node->name.c_str(), sizeof(m_renameBuffer) - 1);
    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    m_focusRenameInput = true;
}

void SceneTreePanel::SetFilterText(const std::string& filter) {
    strncpy(m_searchFilter, filter.c_str(), sizeof(m_searchFilter) - 1);
    m_searchFilter[sizeof(m_searchFilter) - 1] = '\0';
}

void SceneTreePanel::CollectFlattenedNodes(SceneNode* node, std::vector<SceneNode*>& outNodes) {
    if (!node) return;
    outNodes.push_back(node);
    for (const auto& child : node->GetChildren()) {
        CollectFlattenedNodes(child.get(), outNodes);
    }
}

bool SceneTreePanel::NodeMatchesFilter(const SceneNode* node, const std::string& filter) const {
    if (!node || filter.empty()) return true;
    std::string name = node->name;
    std::string q = filter;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name.find(q) != std::string::npos;
}

bool SceneTreePanel::SubtreeMatchesFilter(const SceneNode* node, const std::string& filter) const {
    if (!node) return false;
    if (filter.empty()) return true;
    if (NodeMatchesFilter(node, filter)) return true;
    for (const auto& child : node->GetChildren()) {
        if (SubtreeMatchesFilter(child.get(), filter)) return true;
    }
    return false;
}

void SceneTreePanel::RenderHeaderToolbar(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

    // + Add Node Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.231f, 0.510f, 0.965f, 0.8f));
    if (ImGui::Button("+ Add Node")) {
        ImGui::OpenPopup("AddNodeDropdownPopup");
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Quick Filter / Search Bar
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28.0f);
    ImGui::InputTextWithHint("##SceneFilter", "Filter scene nodes...", m_searchFilter, sizeof(m_searchFilter));

    if (strlen(m_searchFilter) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("x", ImVec2(22.0f, 0))) {
            m_searchFilter[0] = '\0';
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear search filter");
    }

    ImGui::PopStyleVar(2);

    // Popup for Add Node
    if (ImGui::BeginPopup("AddNodeDropdownPopup")) {
        if (rootNode) {
            auto addNodeWithUndo = [this, rootNode](std::unique_ptr<SceneNode> newNode) -> SceneNode* {
                SceneNode* targetParent = (m_selectedNode && m_selectedNode != rootNode) ? m_selectedNode : rootNode;
                if (m_undoStack) {
                    auto cmd = std::make_unique<khepri::scene::AddChildNodeCommand>(targetParent, std::move(newNode));
                    SceneNode* created = cmd->GetCreatedNode();
                    m_undoStack->PushAndExecute(std::move(cmd));
                    SetSelectedNode(created);
                    return created;
                } else {
                    SceneNode* created = targetParent->AddChild(std::move(newNode));
                    SetSelectedNode(created);
                    return created;
                }
            };

            if (ImGui::BeginMenu("3D Primitive Mesh")) {
                if (ImGui::MenuItem("Cube")) {
                    auto newMesh = MeshComponent::CreateCube(m_context, 1.0f);
                    auto newNode = std::make_unique<SceneNode>("Cube");
                    newNode->mesh = newMesh;
                    addNodeWithUndo(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Sphere")) {
                    auto newMesh = MeshComponent::CreateSphere(m_context, 0.6f, 32, 16);
                    auto newNode = std::make_unique<SceneNode>("Sphere");
                    newNode->mesh = newMesh;
                    addNodeWithUndo(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Cylinder")) {
                    auto newMesh = MeshComponent::CreateCylinder(m_context, 0.4f, 1.0f, 32);
                    auto newNode = std::make_unique<SceneNode>("Cylinder");
                    newNode->mesh = newMesh;
                    addNodeWithUndo(std::move(newNode));
                    activeMesh = newMesh;
                }
                if (ImGui::MenuItem("Plane")) {
                    auto newMesh = MeshComponent::CreatePlane(m_context, 4.0f, 8);
                    auto newNode = std::make_unique<SceneNode>("Plane");
                    newNode->mesh = newMesh;
                    addNodeWithUndo(std::move(newNode));
                    activeMesh = newMesh;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light Source")) {
                if (ImGui::MenuItem("Directional Light (Sun)")) {
                    addNodeWithUndo(std::make_unique<DirectionalLightNode>("Directional Light"));
                }
                if (ImGui::MenuItem("Point Light")) {
                    addNodeWithUndo(std::make_unique<PointLightNode>("Point Light"));
                }
                if (ImGui::MenuItem("Spot Light")) {
                    addNodeWithUndo(std::make_unique<SpotLightNode>("Spot Light"));
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Empty Group Node")) {
                addNodeWithUndo(std::make_unique<SceneNode>("Empty Group"));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
}

void SceneTreePanel::HandleMultiSelectClick(SceneNode* node, SceneNode* rootNode) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl) {
        // Toggle selection
        if (IsNodeSelected(node)) {
            DeselectNode(node);
        } else {
            SelectNode(node, true);
        }
    } else if (io.KeyShift && m_lastSelectedNode && rootNode) {
        // Range selection
        std::vector<SceneNode*> flattened;
        CollectFlattenedNodes(rootNode, flattened);

        auto itStart = std::find(flattened.begin(), flattened.end(), m_lastSelectedNode);
        auto itEnd   = std::find(flattened.begin(), flattened.end(), node);

        if (itStart != flattened.end() && itEnd != flattened.end()) {
            if (std::distance(flattened.begin(), itStart) > std::distance(flattened.begin(), itEnd)) {
                std::swap(itStart, itEnd);
            }
            m_selectedNodes.clear();
            for (auto it = itStart; it <= itEnd; ++it) {
                m_selectedNodes.insert(*it);
            }
            m_selectedNode = node;
        } else {
            SelectNode(node, false);
        }
    } else {
        // Single selection
        SetSelectedNode(node);
    }
}

void SceneTreePanel::RenderNodeContextMenu(SceneNode* node, SceneNode* rootNode) {
    if (ImGui::BeginPopupContextItem("NodeContextMenu")) {
        if (!IsNodeSelected(node)) {
            SetSelectedNode(node);
        }

        if (ImGui::MenuItem("Rename", "F2")) {
            StartRenaming(node);
        }

        if (ImGui::MenuItem("Focus Camera", "F")) {
            if (m_onFocusCamera && node) {
                m_onFocusCamera(node->position);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            if (node && node->GetParent()) {
                auto clone = std::make_unique<SceneNode>(node->name + " (Copy)");
                clone->position = node->position + glm::vec3(0.5f, 0.0f, 0.0f);
                clone->rotationDegrees = node->rotationDegrees;
                clone->scale = node->scale;
                clone->mesh = node->mesh;
                clone->lightComponent = node->lightComponent;
                clone->visible = node->visible;

                if (m_undoStack) {
                    auto cmd = std::make_unique<khepri::scene::AddChildNodeCommand>(node->GetParent(), std::move(clone));
                    SceneNode* created = cmd->GetCreatedNode();
                    m_undoStack->PushAndExecute(std::move(cmd));
                    SetSelectedNode(created);
                } else {
                    SceneNode* created = node->GetParent()->AddChild(std::move(clone));
                    SetSelectedNode(created);
                }
            }
        }

        if (node != rootNode && ImGui::MenuItem("Delete Selected", "Del")) {
            std::vector<SceneNode*> toDelete;
            for (auto* sel : m_selectedNodes) {
                if (sel && sel->GetParent() && sel != rootNode) {
                    toDelete.push_back(sel);
                }
            }
            // Filter out nodes whose ancestor is also being deleted
            toDelete.erase(std::remove_if(toDelete.begin(), toDelete.end(), [&](SceneNode* candidate) {
                for (auto* other : toDelete) {
                    if (other != candidate && candidate->IsDescendantOf(other)) {
                        return true;
                    }
                }
                return false;
            }), toDelete.end());

            ClearAllSelections();

            if (m_undoStack) {
                m_undoStack->BeginTransaction("Delete Selected Scene Nodes");
                for (auto* sel : toDelete) {
                    if (sel && sel->GetParent()) {
                        m_undoStack->PushAndExecute(std::make_unique<khepri::scene::RemoveChildNodeCommand>(sel->GetParent(), sel));
                    }
                }
                m_undoStack->EndTransaction();
            } else {
                for (auto* sel : toDelete) {
                    if (sel && sel->GetParent()) {
                        sel->GetParent()->RemoveChild(sel);
                    }
                }
            }
        }

        ImGui::Separator();

        if (node != rootNode && node->GetParent() != rootNode) {
            if (ImGui::MenuItem("Reparent to Root")) {
                if (m_undoStack) {
                    m_undoStack->PushAndExecute(std::make_unique<khepri::scene::ReparentNodeCommand>(node, rootNode));
                } else {
                    auto detached = node->GetParent()->DetachChild(node);
                    if (detached) rootNode->AddChild(std::move(detached));
                }
            }
        }

        ImGui::EndPopup();
    }
}

void SceneTreePanel::RenderNodeTree(SceneNode* node, SceneNode* rootNode, int depth, bool isLastChild, ImVec2 parentPos) {
    if (!node) return;

    std::string filterStr(m_searchFilter);
    if (!filterStr.empty() && !SubtreeMatchesFilter(node, filterStr)) {
        return;
    }

    ImGui::PushID(static_cast<int>(node->id));

    float scale = Theme::GetTotalScale();
    ImVec2 nodeScreenPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw hierarchy connector lines for child levels
    if (depth > 0) {
        ImVec2 branchTarget = ImVec2(nodeScreenPos.x - 6.0f * scale, nodeScreenPos.y + 10.0f * scale);
        UIWidgets::DrawTreeConnectorLine(drawList, parentPos, branchTarget, isLastChild);
    }

    // Determine Vector Icon Type & Color
    VectorIconType iconType = VectorIconType::Mesh;
    ImVec4 iconColor = Theme::COLOR_TEXT_PRIMARY;

    if (node->lightComponent) {
        switch (node->lightComponent->type) {
            case LightType::Directional:
                iconType = VectorIconType::Sun;
                iconColor = ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
                break;
            case LightType::Point:
                iconType = VectorIconType::PointLight;
                iconColor = ImVec4(1.0f, 0.80f, 0.20f, 1.0f);
                break;
            case LightType::Spot:
                iconType = VectorIconType::SpotLight;
                iconColor = ImVec4(1.0f, 0.75f, 0.25f, 1.0f);
                break;
        }
    } else if (node->mesh) {
        iconType = VectorIconType::Mesh;
        iconColor = Theme::COLOR_ACCENT_CYAN;
    } else if (node->GetParent() == nullptr) {
        iconType = VectorIconType::Scene;
        iconColor = Theme::COLOR_TEXT_PRIMARY;
    } else {
        iconType = VectorIconType::Folder;
        iconColor = Theme::COLOR_TEXT_SECONDARY;
    }

    bool isSelected = IsNodeSelected(node);
    bool isNodeRenaming = (m_renamingNode == node);

    // --- Inline Action: Vector Eye Icon (Visibility) ---
    VectorIconType eyeIconType = node->visible ? VectorIconType::Eye : VectorIconType::EyeSlash;
    if (VectorIcons::IconButton("##visBtn", eyeIconType, node->visible,
                               node->visible ? "Visible — click to hide" : "Hidden — click to show",
                               ImVec2(20.0f * scale, 20.0f * scale))) {
        bool newVis = !node->visible;
        if (m_undoStack) {
            m_undoStack->PushAndExecute(std::make_unique<khepri::scene::SetNodeVisibilityCommand>(node, newVis));
        } else {
            node->visible = newVis;
        }
    }

    ImGui::SameLine(0, 4.0f * scale);

    // --- Inline Action: Vector Lock Icon (Transform Scale Lock) ---
    if (node->lockScale || ImGui::IsItemHovered()) {
        VectorIconType lockIconType = node->lockScale ? VectorIconType::Lock : VectorIconType::Unlock;
        if (VectorIcons::IconButton("##lockBtn", lockIconType, node->lockScale,
                                   node->lockScale ? "Scale locked" : "Scale unlocked",
                                   ImVec2(20.0f * scale, 20.0f * scale))) {
            node->lockScale = !node->lockScale;
        }
        ImGui::SameLine(0, 4.0f * scale);
    }

    // --- Tree Node Render ---
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
    if (node->GetParent() == nullptr || !filterStr.empty()) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (node->GetChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    int pushedColors = 0;
    if (!node->visible) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::COLOR_TEXT_MUTED);
        pushedColors++;
    }

    bool opened = false;

    if (isNodeRenaming) {
        // Inline renaming mode
        VectorIcons::RenderInline(iconType, 16.0f * scale, iconColor);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f * scale);
        if (m_focusRenameInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusRenameInput = false;
        }

        if (ImGui::InputText("##InlineRename", m_renameBuffer, sizeof(m_renameBuffer),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (node->name != m_renameBuffer && strlen(m_renameBuffer) > 0) {
                if (m_undoStack) {
                    m_undoStack->PushAndExecute(std::make_unique<khepri::scene::RenameNodeCommand>(node, std::string(m_renameBuffer)));
                } else {
                    node->name = m_renameBuffer;
                }
            }
            m_renamingNode = nullptr;
        } else if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (ImGui::IsItemDeactivatedAfterEdit() && node->name != m_renameBuffer && strlen(m_renameBuffer) > 0) {
                if (m_undoStack) {
                    m_undoStack->PushAndExecute(std::make_unique<khepri::scene::RenameNodeCommand>(node, std::string(m_renameBuffer)));
                } else {
                    node->name = m_renameBuffer;
                }
            }
            m_renamingNode = nullptr;
        }
    } else {
        // Render tree row with vector icon and node name
        ImVec2 treeCursor = ImGui::GetCursorScreenPos();
        opened = ImGui::TreeNodeEx("##TreeNode", flags, "    %s", node->name.c_str());

        // Draw crisp vector icon over tree row indentation
        ImVec2 iconCenter(treeCursor.x + (node->GetChildren().empty() ? 10.0f : 30.0f) * scale, treeCursor.y + 10.0f * scale);
        VectorIcons::Draw(drawList, iconType, iconCenter, 14.0f * scale, ImGui::GetColorU32(iconColor));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            HandleMultiSelectClick(node, rootNode);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                StartRenaming(node);
            }
        }

        // F2 Shortcut for renaming currently selected node
        if (isSelected && ImGui::IsKeyPressed(ImGuiKey_F2)) {
            StartRenaming(node);
        }
    }

    if (pushedColors > 0) {
        ImGui::PopStyleColor(pushedColors);
    }

    // --- Drag and Drop Source ---
    if (node->GetParent() != nullptr && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        uint32_t nodeId = node->id;
        ImGui::SetDragDropPayload("SCENE_NODE_ID", &nodeId, sizeof(uint32_t));
        ImGui::Text("Moving %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }

    // --- Drag and Drop Target ---
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE_ID")) {
            uint32_t draggedId = *static_cast<const uint32_t*>(payload->Data);
            SceneNode* draggedNode = rootNode ? rootNode->FindDescendantById(draggedId) : nullptr;
            if (draggedNode && draggedNode != node && draggedNode->GetParent() != node && !node->IsDescendantOf(draggedNode)) {
                if (m_undoStack) {
                    m_undoStack->PushAndExecute(std::make_unique<khepri::scene::ReparentNodeCommand>(draggedNode, node));
                } else {
                    SceneNode* oldParent = draggedNode->GetParent();
                    if (oldParent) {
                        auto detached = oldParent->DetachChild(draggedNode);
                        if (detached) {
                            node->AddChild(std::move(detached));
                        }
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

    RenderNodeContextMenu(node, rootNode);

    // Recurse children
    if (opened) {
        ImVec2 currentChildAnchor = ImVec2(nodeScreenPos.x + 12.0f * scale, nodeScreenPos.y + 14.0f * scale);
        const auto& children = node->GetChildren();
        for (size_t i = 0; i < children.size(); ++i) {
            bool isLast = (i == children.size() - 1);
            RenderNodeTree(children[i].get(), rootNode, depth + 1, isLast, currentChildAnchor);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SceneTreePanel::RenderFileAssetInspector(const std::filesystem::path& assetPath) {
    if (!std::filesystem::exists(assetPath)) {
        ImGui::TextDisabled("Selected file no longer exists.");
        return;
    }

    std::string filename = assetPath.filename().string();
    std::string ext = assetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    Theme::PushFontHeader();
    ImGui::TextColored(Theme::COLOR_ACCENT_CYAN, "Asset Inspector");
    Theme::PopFont();
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

    ImGui::Separator();

    bool isModel = (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".stl");
    if (isModel) {
        UIWidgets::DrawBadge("3D MODEL", Theme::COLOR_BADGE_MODEL, Theme::COLOR_BADGE_MODEL_TEXT);
        ImGui::Spacing();
        if (ImGui::Button("Open Scene with Model (Replace Current)", ImVec2(-1, 30))) {
            if (m_onOpenModel) m_onOpenModel(assetPath.string());
        }
        if (ImGui::Button("Import Model into Current Scene", ImVec2(-1, 30))) {
            if (m_onImportModel) m_onImportModel(assetPath.string());
        }
    } else if (ext == ".mat") {
        UIWidgets::DrawBadge("MATERIAL", Theme::COLOR_BADGE_MATERIAL, Theme::COLOR_BADGE_MATERIAL_TEXT);
        ImGui::Spacing();
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
        UIWidgets::DrawBadge("DOCUMENT", ImVec4(0.2f, 0.2f, 0.2f, 0.3f), Theme::COLOR_TEXT_SECONDARY);
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

void SceneTreePanel::RenderInspector(SceneNode* node, const std::filesystem::path& selectedAssetPath,
                                     khepri::NodeGraphEditorPanel* nodeGraphPanel) {
    if (!node) return;
    (void)selectedAssetPath;

    char nameBuf[256];
    strncpy(nameBuf, node->name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Node Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (m_undoStack && node->name != nameBuf) {
            m_undoStack->PushAndExecute(std::make_unique<khepri::scene::RenameNodeCommand>(node, std::string(nameBuf)));
        } else {
            node->name = nameBuf;
        }
    } else if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (node->name != nameBuf) {
            if (m_undoStack) {
                m_undoStack->PushAndExecute(std::make_unique<khepri::scene::RenameNodeCommand>(node, std::string(nameBuf)));
            } else {
                node->name = nameBuf;
            }
        }
    }

    bool vis = node->visible;
    if (ImGui::Checkbox("Visible", &vis)) {
        if (m_undoStack) {
            m_undoStack->PushAndExecute(std::make_unique<khepri::scene::SetNodeVisibilityCommand>(node, vis));
        } else {
            node->visible = vis;
        }
    }

    ImGui::Separator();
    Theme::PushFontHeader();
    ImGui::TextColored(Theme::COLOR_ACCENT_CYAN, "Transform");
    Theme::PopFont();

    // Position
    if (ImGui::DragFloat3("Position", glm::value_ptr(node->position), 0.1f, -100.0f, 100.0f)) {
        node->SyncPropertiesToTransform();
    }
    if (ImGui::IsItemActivated()) {
        m_dragStartPos = node->position;
        m_dragStartRot = node->rotationDegrees;
        m_dragStartScale = node->scale;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (m_undoStack && m_dragStartPos != node->position) {
            m_undoStack->Push(std::make_unique<khepri::scene::TransformCommand>(
                node, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                node->position, node->rotationDegrees, node->scale));
        }
    }

    // Rotation
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(node->rotationDegrees), 0.1f, -360.0f, 360.0f)) {
        node->SyncPropertiesToTransform();
    }
    if (ImGui::IsItemActivated()) {
        m_dragStartPos = node->position;
        m_dragStartRot = node->rotationDegrees;
        m_dragStartScale = node->scale;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (m_undoStack && m_dragStartRot != node->rotationDegrees) {
            m_undoStack->Push(std::make_unique<khepri::scene::TransformCommand>(
                node, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                node->position, node->rotationDegrees, node->scale));
        }
    }

    // Scale
    bool isLightNode = (node->lightComponent != nullptr);
    if (isLightNode) {
        ImGui::BeginDisabled();
    }
    VectorIcons::RenderInline(VectorIconType::Lock, 14.0f * Theme::GetTotalScale(), Theme::COLOR_TEXT_PRIMARY);
    ImGui::SameLine();
    ImGui::Checkbox("Lock Scale Aspect Ratio", &node->lockScale);
    glm::vec3 oldScale = node->scale;
    glm::vec3 newScale = oldScale;
    if (ImGui::DragFloat3("Scale", glm::value_ptr(newScale), 0.01f, 0.001f, 100.0f)) {
        if (!isLightNode) {
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
    }
    if (isLightNode) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Scale is disabled for light nodes. Light volume is governed by Range (m) and Cutoff Angles below.");
        }
    } else {
        if (ImGui::IsItemActivated()) {
            m_dragStartPos = node->position;
            m_dragStartRot = node->rotationDegrees;
            m_dragStartScale = node->scale;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (m_undoStack && m_dragStartScale != node->scale) {
                m_undoStack->Push(std::make_unique<khepri::scene::TransformCommand>(
                    node, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                    node->position, node->rotationDegrees, node->scale));
            }
        }
    }

    if (node->lightComponent) {
        ImGui::Separator();
        Theme::PushFontHeader();
        ImGui::TextColored(Theme::COLOR_ACCENT_CYAN, "Light Component");
        Theme::PopFont();

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
            ImGui::DragFloat("Range (m)", &node->lightComponent->range, 0.1f, 0.1f, 100.0f, "%.1f m");
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
        Theme::PushFontHeader();
        ImGui::TextColored(Theme::COLOR_ACCENT_CYAN, "Mesh Material & Topology");
        Theme::PopFont();
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

    // Node Graph Properties section (shows when a graph node is also selected)
    if (nodeGraphPanel && nodeGraphPanel->GetSelectedNodeId() != 0) {
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Node Graph Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            nodeGraphPanel->RenderNodePropertiesInspector();
        }
    }
}

void SceneTreePanel::RenderUI(SceneNode* rootNode, std::shared_ptr<MeshComponent>& activeMesh,
                              const std::filesystem::path& selectedAssetPath,
                              khepri::NodeGraphEditorPanel* nodeGraphPanel) {
    ValidateSelection(rootNode);

    ImGui::Begin("Scene Hierarchy");

    RenderHeaderToolbar(rootNode, activeMesh);

    // --- Scene Tree ---
    if (rootNode) {
        ImGui::BeginChild("SceneTreeScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        RenderNodeTree(rootNode, rootNode, 0, false, ImGui::GetCursorScreenPos());
        ImGui::EndChild();
    }

    // Drop target on empty space
    if (rootNode && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE_ID")) {
            uint32_t draggedId = *static_cast<const uint32_t*>(payload->Data);
            SceneNode* draggedNode = rootNode->FindDescendantById(draggedId);
            if (draggedNode && draggedNode->GetParent() != rootNode && draggedNode != rootNode) {
                if (m_undoStack) {
                    m_undoStack->PushAndExecute(std::make_unique<khepri::scene::ReparentNodeCommand>(draggedNode, rootNode));
                } else {
                    SceneNode* oldParent = draggedNode->GetParent();
                    if (oldParent) {
                        auto detached = oldParent->DetachChild(draggedNode);
                        if (detached) {
                            rootNode->AddChild(std::move(detached));
                        }
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
        RenderInspector(m_selectedNode, selectedAssetPath, nodeGraphPanel);
    } else if (nodeGraphPanel && nodeGraphPanel->GetSelectedNodeId() != 0) {
        Theme::PushFontHeader();
        ImGui::TextColored(Theme::COLOR_ACCENT_CYAN, "Node Graph Properties");
        Theme::PopFont();
        ImGui::Separator();
        nodeGraphPanel->RenderNodePropertiesInspector();
    } else if (!selectedAssetPath.empty()) {
        RenderFileAssetInspector(selectedAssetPath);
    } else {
        ImGui::TextDisabled("No Node or Asset File Selected");
    }
    ImGui::End();
}

#include "EditorUISubsystem.h"
#include "../core/Logger.h"
#include "../core/FileDialog.h"
#include "../core/Version.h"
#include "../core/Window.h"
#include "../vulkan/VulkanContext.h"
#include "../vulkan/Swapchain.h"
#include "../assets/AssetManager.h"
#include "../scene/SceneNode.h"
#include "../scene/MeshComponent.h"
#include "../core/UndoStack.h"
#include "OracleBridge.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <cstring>

namespace khepri {

EditorUISubsystem::~EditorUISubsystem() {
    Shutdown();
}

void EditorUISubsystem::Initialize(EngineContext& context) {
    m_ctx = &context;

    InitImGui();

    if (!m_ctx->vulkanContext) return;

    // 1. Initialize AssetManager & Node Graph Engine
    khepri::AssetManager::Instance().Initialize(*m_ctx->vulkanContext);

    // 2. Instantiate Editor Panels
    m_viewportPanel = std::make_unique<ViewportPanel>(*m_ctx->vulkanContext);
    if (m_ctx->undoStack) {
        m_viewportPanel->GetGizmo().SetUndoStack(m_ctx->undoStack);
    }

    m_sceneTreePanel = std::make_unique<SceneTreePanel>(*m_ctx->vulkanContext);
    if (m_ctx->undoStack) {
        m_sceneTreePanel->SetUndoStack(m_ctx->undoStack);
    }

    m_timelinePanel = std::make_unique<TimelinePanel>();
    m_vulkanInspectorPanel = std::make_unique<VulkanInspectorPanel>(*m_ctx->vulkanContext);

    m_nodeGraphEditorPanel = std::make_unique<khepri::NodeGraphEditorPanel>(*m_ctx->vulkanContext);
    if (m_ctx->undoStack) {
        m_nodeGraphEditorPanel->SetUndoStack(m_ctx->undoStack);
    }

    m_assetManagerPanel = std::make_unique<khepri::AssetManagerPanel>();

    // 3. Connect Asset & Viewport callbacks to EngineContext action handlers
    m_assetManagerPanel->SetOpenModelCallback([this](const std::string& path) {
        if (m_ctx && m_ctx->openSceneModel) m_ctx->openSceneModel(path);
    });
    m_assetManagerPanel->SetImportModelCallback([this](const std::string& path) {
        if (m_ctx && m_ctx->importModelIntoScene) m_ctx->importModelIntoScene(path);
    });

    m_viewportPanel->SetImportModelCallback([this](const std::string& path) {
        if (m_ctx && m_ctx->importModelIntoScene) m_ctx->importModelIntoScene(path);
    });
    m_viewportPanel->SetSelectNodeCallback([this](SceneNode* node) {
        if (m_sceneTreePanel) {
            m_sceneTreePanel->SetSelectedNode(node);
        }
    });

    m_sceneTreePanel->SetImportModelCallback([this](const std::string& path) {
        if (m_ctx && m_ctx->importModelIntoScene) m_ctx->importModelIntoScene(path);
    });
    m_sceneTreePanel->SetOpenModelCallback([this](const std::string& path) {
        if (m_ctx && m_ctx->openSceneModel) m_ctx->openSceneModel(path);
    });

    // 4. Centralized Undo/Redo state observer
    if (m_ctx->undoStack) {
        m_ctx->undoStack->SetChangeListener([this]() {
            if (!m_ctx) return;
            SceneNode* root = m_ctx->rootNode ? m_ctx->rootNode->get() : nullptr;
            if (m_sceneTreePanel) {
                m_sceneTreePanel->ValidateSelection(root);
            }
            if (m_nodeGraphEditorPanel) {
                m_nodeGraphEditorPanel->ValidateTargetSceneNode(root);
                SceneNode* sel = m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr;
                if (m_nodeGraphEditorPanel->GetTargetSceneNode() != sel) {
                    m_nodeGraphEditorPanel->SetTargetSceneNode(sel);
                }
                if (sel && sel->nodeGraph) {
                    sel->nodeGraph->Evaluate();
                    if (auto outMesh = m_nodeGraphEditorPanel->GetActiveOutputMesh()) {
                        if (!sel->lightComponent) sel->mesh = outMesh;
                    }
                }
            }
        });
    }

    m_initialized = true;
    LOG_INFO("EditorUISubsystem: Initialized all editor panels and UI state successfully.");
}

void EditorUISubsystem::InitImGui() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    vkCreateDescriptorPool(m_ctx->vulkanContext->GetDevice(), &poolInfo, nullptr, &m_imguiPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_ctx->window->GetNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_ctx->vulkanContext->GetInstance();
    initInfo.PhysicalDevice = m_ctx->vulkanContext->GetPhysicalDevice();
    initInfo.Device = m_ctx->vulkanContext->GetDevice();
    initInfo.QueueFamily = m_ctx->vulkanContext->GetQueueFamilies().graphicsFamily.value();
    initInfo.Queue = m_ctx->vulkanContext->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = m_ctx->swapchain->GetImageFormat();
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

    ImGui_ImplVulkan_Init(&initInfo);
}

void EditorUISubsystem::Update([[maybe_unused]] float deltaTime) {
    if (!m_ctx || !m_ctx->window || !m_ctx->undoStack) return;

    // Global Keyboard Shortcuts (Undo: Ctrl+Z, Redo: Ctrl+Y / Ctrl+Shift+Z, Fullscreen: F11 / Alt+Enter)
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F11, false) ||
            (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            m_ctx->window->ToggleFullscreen();
        } else if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (m_ctx->undoStack->CanUndo()) {
                m_ctx->undoStack->Undo();
            }
        } else if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
                   (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
            if (m_ctx->undoStack->CanRedo()) {
                m_ctx->undoStack->Redo();
            }
        }
    }
}

void EditorUISubsystem::RenderUI() {
    if (!m_ctx) return;

    // 1. Begin ImGui Frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    if (m_ctx->oracleBridge && m_ctx->window) {
        m_ctx->oracleBridge->PollEvents(ImGui::GetIO(), m_ctx->window->GetWidth(), m_ctx->window->GetHeight());
    }
    ImGui::NewFrame();

    ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    RenderMainMenuBar(dockspaceID);

    if (m_rebuildLayout) {
        m_rebuildLayout = false;
        ApplyDockLayout(dockspaceID);
    }

    SceneNode* root = m_ctx->rootNode ? m_ctx->rootNode->get() : nullptr;
    std::shared_ptr<MeshComponent> activeMesh = m_ctx->activeDisplayMesh ? *m_ctx->activeDisplayMesh : nullptr;

    // 2. Render UI panels
    if (m_showViewport && m_viewportPanel && m_ctx->camera) {
        m_viewportPanel->RenderUI(*m_ctx->camera, m_currentFrame, m_deltaTime,
                                  m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr,
                                  root);
    }
    if (m_showSceneTree && m_sceneTreePanel) {
        const auto& selectedAsset = m_assetManagerPanel ? m_assetManagerPanel->GetSelectedPath() : std::filesystem::path();
        m_sceneTreePanel->RenderUI(root, activeMesh, selectedAsset,
                                   m_nodeGraphEditorPanel.get());
    }
    if (m_showTimeline && m_timelinePanel && m_ctx->timeline) {
        m_timelinePanel->RenderUI(*m_ctx->timeline,
                                  m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr,
                                  root);
    }
    if (m_showVulkanInspector && m_vulkanInspectorPanel && m_ctx->swapchain) {
        m_vulkanInspectorPanel->RenderUI(*m_ctx->swapchain);
    }
    if (m_showNodeGraph && m_nodeGraphEditorPanel) {
        m_nodeGraphEditorPanel->ValidateTargetSceneNode(root);
        SceneNode* selected = m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr;
        if (m_nodeGraphEditorPanel->GetTargetSceneNode() != selected) {
            m_nodeGraphEditorPanel->SetTargetSceneNode(selected);
        }

        m_nodeGraphEditorPanel->RenderUI(activeMesh);
        
        // Synchronize active graph output mesh to target SceneNode (excluding lights)
        if (selected && selected->mesh && activeMesh && !selected->lightComponent) {
            selected->mesh = activeMesh;
        }
    }
    if (m_showAssetManager && m_assetManagerPanel) {
        m_assetManagerPanel->RenderUI(&m_showAssetManager);
    }
    if (m_showLogConsole) {
        RenderEngineLogConsole(&m_showLogConsole);
    }

    ImGui::Render();
}

void EditorUISubsystem::Shutdown() {
    if (!m_initialized) return;

    LOG_INFO("EditorUISubsystem: Executing hierarchical teardown...");

    // 1. Destroy all child panels and their ImGui/Vulkan resources FIRST
    m_sceneTreePanel.reset();
    m_timelinePanel.reset();
    m_nodeGraphEditorPanel.reset();
    m_vulkanInspectorPanel.reset();
    m_assetManagerPanel.reset();
    m_viewportPanel.reset();

    // 2. Shut down ImGui backends once child panels are destroyed
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // 3. Destroy ImGui descriptor pool
    if (m_ctx) {
        if (m_ctx->undoStack) {
            m_ctx->undoStack->SetChangeListener(nullptr);
        }
        if (m_imguiPool != VK_NULL_HANDLE) {
            if (m_ctx->vulkanContext && m_ctx->vulkanContext->GetDevice() != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_ctx->vulkanContext->GetDevice(), m_imguiPool, nullptr);
            }
            m_imguiPool = VK_NULL_HANDLE;
        }
    }

    m_ctx = nullptr;
    m_initialized = false;
    LOG_INFO("EditorUISubsystem: Teardown complete.");
}

void EditorUISubsystem::OnSceneLoaded(SceneNode* root, const std::shared_ptr<MeshComponent>& mesh) {
    if (m_sceneTreePanel) {
        m_sceneTreePanel->ClearSelectedNode();
    }
    if (root) {
        const auto& children = root->GetChildren();
        if (!children.empty() && children[0]->mesh) {
            if (m_sceneTreePanel) {
                m_sceneTreePanel->SetSelectedNode(children[0].get());
            }
            if (m_nodeGraphEditorPanel) {
                m_nodeGraphEditorPanel->SetTargetSceneNode(children[0].get());
                m_nodeGraphEditorPanel->SetImportedMesh(mesh);
            }
        }
    }
}

void EditorUISubsystem::OnModelImported(SceneNode* newlyAdded, const std::shared_ptr<MeshComponent>& mesh) {
    if (newlyAdded && m_sceneTreePanel) {
        m_sceneTreePanel->SetSelectedNode(newlyAdded);
    }
    if (mesh && m_nodeGraphEditorPanel && newlyAdded) {
        m_nodeGraphEditorPanel->SetTargetSceneNode(newlyAdded);
        m_nodeGraphEditorPanel->SetImportedMesh(mesh);
    }
}

void EditorUISubsystem::ClearSelection() {
    if (m_sceneTreePanel) {
        m_sceneTreePanel->ClearSelectedNode();
    }
}

void EditorUISubsystem::SetSelectedNode(SceneNode* node) {
    if (m_sceneTreePanel) {
        m_sceneTreePanel->SetSelectedNode(node);
    }
}

void EditorUISubsystem::ApplyDockLayout(ImGuiID dockspaceID) {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

    ImGuiID dockMain = dockspaceID;
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, nullptr, &dockMain);
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.22f, nullptr, &dockMain);

    ImGuiID dockRightTop = dockRight;
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRightTop, ImGuiDir_Down, 0.45f, nullptr, &dockRightTop);

    ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
    ImGui::DockBuilderDockWindow("3D Viewport", dockMain);
    ImGui::DockBuilderDockWindow("Inspector", dockRightTop);
    ImGui::DockBuilderDockWindow("Vulkan Educational Inspector", dockRightBottom);
    ImGui::DockBuilderDockWindow("Animation Timeline", dockBottom);
    ImGui::DockBuilderDockWindow("Engine Log Console", dockBottom);
    ImGui::DockBuilderDockWindow("Procedural Node Graph Editor", dockBottom);
    ImGui::DockBuilderDockWindow("Asset Manager", dockBottom);

    ImGui::DockBuilderFinish(dockspaceID);
}

void EditorUISubsystem::RenderMainMenuBar([[maybe_unused]] ImGuiID dockspaceID) {
    if (!m_ctx) return;

    if (ImGui::BeginMainMenuBar()) {
        std::string versionTitle = std::string("Khepri Engine v") + KhepriEngine::VERSION_STRING;
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", versionTitle.c_str());
        ImGui::Separator();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Scene / 3D Model...")) {
                std::string selectedPath = FileDialog::OpenFile();
                if (!selectedPath.empty() && m_ctx->openSceneModel) {
                    m_ctx->openSceneModel(selectedPath);
                }
            }
            if (ImGui::MenuItem("Import 3D Model into Current Scene...")) {
                std::string selectedPath = FileDialog::OpenFile();
                if (!selectedPath.empty() && m_ctx->importModelIntoScene) {
                    m_ctx->importModelIntoScene(selectedPath);
                }
            }
            if (ImGui::MenuItem("Enter Model Path...")) {
                m_openGltfModal = true;
            }
            if (ImGui::BeginMenu("Sample 3D Models")) {
                if (ImGui::MenuItem("Box (glTF)")) {
                    if (m_ctx->openSceneModel) m_ctx->openSceneModel("assets/models/Box.gltf");
                }
                if (ImGui::MenuItem("Sphere")) {
                    if (m_ctx->openSceneModel) m_ctx->openSceneModel("Sphere");
                }
                if (ImGui::MenuItem("Cylinder")) {
                    if (m_ctx->openSceneModel) m_ctx->openSceneModel("Cylinder");
                }
                if (ImGui::MenuItem("Plane")) {
                    if (m_ctx->openSceneModel) m_ctx->openSceneModel("Plane");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            std::string undoLabel = "Undo";
            bool canUndo = m_ctx->undoStack && m_ctx->undoStack->CanUndo();
            if (canUndo) {
                undoLabel += " " + std::string(m_ctx->undoStack->GetUndoCommandName());
            }
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo)) {
                if (m_ctx->undoStack) m_ctx->undoStack->Undo();
            }

            std::string redoLabel = "Redo";
            bool canRedo = m_ctx->undoStack && m_ctx->undoStack->CanRedo();
            if (canRedo) {
                redoLabel += " " + std::string(m_ctx->undoStack->GetRedoCommandName());
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo)) {
                if (m_ctx->undoStack) m_ctx->undoStack->Redo();
            }

            ImGui::Separator();
            bool hasHistory = m_ctx->undoStack && (m_ctx->undoStack->GetUndoCount() > 0 || m_ctx->undoStack->GetRedoCount() > 0);
            if (ImGui::MenuItem("Clear Undo History", nullptr, false, hasHistory)) {
                if (m_ctx->undoStack) m_ctx->undoStack->Clear();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Camera View (F)")) {
                if (m_ctx->camera) m_ctx->camera->FocusOnTarget(glm::vec3(0.0f));
            }
            bool gridVis = m_viewportPanel ? m_viewportPanel->IsGridVisible() : true;
            if (ImGui::MenuItem("Show 3D Ground Grid", "G", &gridVis)) {
                if (m_viewportPanel) m_viewportPanel->SetGridVisible(gridVis);
            }
            bool isFs = m_ctx->window && m_ctx->window->IsFullscreen();
            if (ImGui::MenuItem("Toggle Fullscreen", "F11", isFs)) {
                if (m_ctx->window) m_ctx->window->ToggleFullscreen();
            }
            if (ImGui::MenuItem("Reset Layout")) {
                m_rebuildLayout = true;
            }
            if (ImGui::BeginMenu("Set Application Resolution")) {
                if (ImGui::MenuItem("1280 x 720 (720p HD)")) {
                    if (m_ctx->window) glfwSetWindowSize(m_ctx->window->GetNativeWindow(), 1280, 720);
                }
                if (ImGui::MenuItem("1600 x 900 (900p HD+)")) {
                    if (m_ctx->window) glfwSetWindowSize(m_ctx->window->GetNativeWindow(), 1600, 900);
                }
                if (ImGui::MenuItem("1920 x 1080 (1080p Full HD)")) {
                    if (m_ctx->window) glfwSetWindowSize(m_ctx->window->GetNativeWindow(), 1920, 1080);
                }
                if (ImGui::MenuItem("2560 x 1440 (1440p QHD)")) {
                    if (m_ctx->window) glfwSetWindowSize(m_ctx->window->GetNativeWindow(), 2560, 1440);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showSceneTree);
            ImGui::MenuItem("Node Graph Editor", nullptr, &m_showNodeGraph);
            ImGui::MenuItem("Timeline / Animation", nullptr, &m_showTimeline);
            ImGui::MenuItem("Asset Manager", nullptr, &m_showAssetManager);
            ImGui::MenuItem("Vulkan Inspector", nullptr, &m_showVulkanInspector);
            ImGui::MenuItem("Engine Log Console", nullptr, &m_showLogConsole);
            ImGui::Separator();
            if (ImGui::MenuItem("Show All Windows")) {
                m_showViewport = m_showSceneTree = m_showNodeGraph = m_showTimeline = m_showAssetManager = m_showVulkanInspector = m_showLogConsole = true;
            }
            if (ImGui::MenuItem("Hide All Windows")) {
                m_showViewport = m_showSceneTree = m_showNodeGraph = m_showTimeline = m_showAssetManager = m_showVulkanInspector = m_showLogConsole = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Khepri Engine")) {
                ImGui::OpenPopup("About Khepri Engine Modal");
            }
            ImGui::EndMenu();
        }

        // Model import path modal
        if (m_openGltfModal) {
            ImGui::OpenPopup("Import 3D Model Path");
            m_openGltfModal = false;
        }

        if (ImGui::BeginPopupModal("Import 3D Model Path", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter local filepath or browse for .gltf / .glb / .obj / .stl file:");
            ImGui::InputText("Path", m_gltfPathInput, sizeof(m_gltfPathInput));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                std::string selectedPath = FileDialog::OpenFile();
                if (!selectedPath.empty()) {
                    strncpy_s(m_gltfPathInput, sizeof(m_gltfPathInput), selectedPath.c_str(), _TRUNCATE);
                }
            }
            if (ImGui::Button("Load Model", ImVec2(120, 0))) {
                if (m_ctx->openSceneModel) m_ctx->openSceneModel(m_gltfPathInput);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("About Khepri Engine Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Khepri Engine v1.0");
            ImGui::Text("Data-Oriented Vulkan Graphics & Computational Geometry Engine");
            ImGui::Separator();
            ImGui::Text("• Control Scheme: Unreal Engine Flycam (RMB+WASDQE) & MeshLab Trackball");
            ImGui::Text("• Asset Importer: glTF 2.0 (.gltf / .glb)");
            ImGui::Text("• Architecture: Subsystem-Based Hierarchical Ownership & Two-Tiered Mesh Topology");
            ImGui::Text("• Performance: Double-buffered in-flight offscreen targets & zero-copy compute pipelines");
            ImGui::Separator();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorUISubsystem::RenderEngineLogConsole(bool* p_open) {
    if (p_open && !*p_open) return;
    if (!ImGui::Begin("Engine Log Console", p_open)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear Logs")) {
        Logger::Get().ClearLogs();
    }
    ImGui::SameLine();
    static bool autoScroll = true;
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& log : Logger::Get().GetLogs()) {
        ImVec4 color(0.9f, 0.9f, 0.9f, 1.0f);
        if (log.level == LogLevel::Warning)     color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        else if (log.level == LogLevel::Error)  color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        else if (log.level == LogLevel::VulkanDebug) color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);

        ImGui::TextColored(color, "[%s] %s", log.timestamp.c_str(), log.message.c_str());
    }
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace khepri
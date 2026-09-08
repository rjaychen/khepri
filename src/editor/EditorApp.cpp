#include "EditorApp.h"
#include "../core/Logger.h"
#include "../core/FileDialog.h"
#include "../mesh/GLTFImporter.h"
#include "../assets/AssetManager.h"
#include "../scene/LightNode.h"
#include "../vulkan/VulkanUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <fstream>
#include <array>
#include <imgui_internal.h>

void EditorApp::ApplyDockLayout(ImGuiID dockspaceID) {
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

void EditorApp::SetEditorMode(EditorMode mode) {
    m_currentMode = mode;
}

void EditorApp::OpenSceneModel(const std::string& path) {
    m_context->WaitIdle();
    if (m_sceneTreePanel) {
        m_sceneTreePanel->ClearSelectedNode();
    }
    VkBuffer lightBuf = m_lightUBOBuffer ? m_lightUBOBuffer->GetBuffer() : VK_NULL_HANDLE;
    auto loadedNodeResult = ModelImporter::LoadFromFile(*m_context, path, m_textureDescriptorSetLayout, m_descriptorAllocator.get(), lightBuf);
    if (loadedNodeResult.has_value()) {
        m_rootNode = loadedNodeResult.value();

        // Ensure scene root has a default sun light
        m_rootNode->AddChild(std::make_unique<DirectionalLightNode>("Sun / Main Light", glm::vec3(5.0f, 10.0f, 5.0f), glm::vec3(-45.0f, 45.0f, 0.0f)));

        const auto& children = m_rootNode->GetChildren();
        if (!children.empty() && children[0]->mesh) {
            m_activeDisplayMesh = children[0]->mesh;
            if (m_sceneTreePanel) {
                m_sceneTreePanel->SetSelectedNode(children[0].get());
            }
            if (m_nodeGraphEditorPanel) {
                m_nodeGraphEditorPanel->SetTargetSceneNode(children[0].get());
                m_nodeGraphEditorPanel->SetImportedMesh(m_activeDisplayMesh);
            }
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Opened new scene from model: " + path);
    } else {
        LOG_ERROR("Failed to open scene model from: " + path + " - " + std::string(khepri::ToString(loadedNodeResult.error())));
    }
}

void EditorApp::ImportModelIntoScene(const std::string& path) {
    m_context->WaitIdle();
    VkBuffer lightBuf = m_lightUBOBuffer ? m_lightUBOBuffer->GetBuffer() : VK_NULL_HANDLE;
    auto loadedNodeResult = ModelImporter::LoadFromFile(*m_context, path, m_textureDescriptorSetLayout, m_descriptorAllocator.get(), lightBuf);
    if (loadedNodeResult.has_value()) {
        auto loadedNode = loadedNodeResult.value();
        if (!m_rootNode) {
            m_rootNode = std::make_shared<SceneNode>("Scene Root");
        }

        std::string stemName = std::filesystem::path(path).stem().string();
        if (stemName.empty()) stemName = "Imported Model";

        SceneNode* newlyAdded = nullptr;
        const auto& loadedChildren = loadedNode->GetChildren();
        if (!loadedChildren.empty()) {
            for (const auto& child : loadedChildren) {
                if (child->mesh) {
                    m_activeDisplayMesh = child->mesh;
                    std::string nodeName = (loadedChildren.size() == 1) ? stemName : (stemName + " (" + child->name + ")");
                    auto importedChild = std::make_unique<SceneNode>(nodeName);
                    importedChild->mesh = child->mesh;
                    newlyAdded = m_rootNode->AddChild(std::move(importedChild));
                }
            }
        } else if (loadedNode->mesh) {
            m_activeDisplayMesh = loadedNode->mesh;
            auto importedChild = std::make_unique<SceneNode>(stemName);
            importedChild->mesh = loadedNode->mesh;
            newlyAdded = m_rootNode->AddChild(std::move(importedChild));
        }

        if (newlyAdded && m_sceneTreePanel) {
            m_sceneTreePanel->SetSelectedNode(newlyAdded);
        }

        if (m_activeDisplayMesh && m_nodeGraphEditorPanel && newlyAdded) {
            m_nodeGraphEditorPanel->SetTargetSceneNode(newlyAdded);
            m_nodeGraphEditorPanel->SetImportedMesh(m_activeDisplayMesh);
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Imported model into current scene: " + path);
    } else {
        LOG_ERROR("Failed to import model into scene: " + path + " - " + std::string(khepri::ToString(loadedNodeResult.error())));
    }
}

void EditorApp::LoadSampleModel(const std::string& name) {
    m_context->WaitIdle();
    if (m_sceneTreePanel) {
        m_sceneTreePanel->ClearSelectedNode();
    }
    if (name == "Box") {
        OpenSceneModel("assets/models/Box.gltf");
    } else {
        m_activeDisplayMesh = GLTFImporter::CreateSampleMesh(*m_context, name);
        m_rootNode = std::make_shared<SceneNode>("Scene Root");
        auto childNode = std::make_unique<SceneNode>(name + " Node");
        childNode->mesh = m_activeDisplayMesh;
        SceneNode* addedChild = m_rootNode->AddChild(std::move(childNode));
        if (m_sceneTreePanel) {
            m_sceneTreePanel->SetSelectedNode(addedChild);
        }
        if (m_nodeGraphEditorPanel) {
            m_nodeGraphEditorPanel->SetTargetSceneNode(addedChild);
            m_nodeGraphEditorPanel->SetImportedMesh(m_activeDisplayMesh);
        }
        m_camera.FocusOnTarget(glm::vec3(0.0f), 4.0f);
        LOG_INFO("Loaded sample primitive model: " + name);
    }
}

void EditorApp::RenderMainMenuBar(ImGuiID dockspaceID) {
    (void)dockspaceID;
    if (ImGui::BeginMainMenuBar()) {
        std::string versionTitle = std::string("Khepri Engine v") + KhepriEngine::VERSION_STRING;
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", versionTitle.c_str());
        ImGui::Separator();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Scene / 3D Model...")) {
                std::string selectedPath = FileDialog::OpenFile();
                if (!selectedPath.empty()) {
                    OpenSceneModel(selectedPath);
                }
            }
            if (ImGui::MenuItem("Import 3D Model into Current Scene...")) {
                std::string selectedPath = FileDialog::OpenFile();
                if (!selectedPath.empty()) {
                    ImportModelIntoScene(selectedPath);
                }
            }
            if (ImGui::MenuItem("Enter Model Path...")) {
                m_openGltfModal = true;
            }
            if (ImGui::BeginMenu("Sample 3D Models")) {
                if (ImGui::MenuItem("Box (glTF)"))  LoadSampleModel("Box");
                if (ImGui::MenuItem("Sphere"))       LoadSampleModel("Sphere");
                if (ImGui::MenuItem("Cylinder"))     LoadSampleModel("Cylinder");
                if (ImGui::MenuItem("Plane"))        LoadSampleModel("Plane");
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            std::string undoLabel = "Undo";
            if (m_undoStack.CanUndo()) {
                undoLabel += " " + std::string(m_undoStack.GetUndoCommandName());
            }
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, m_undoStack.CanUndo())) {
                m_undoStack.Undo();
            }

            std::string redoLabel = "Redo";
            if (m_undoStack.CanRedo()) {
                redoLabel += " " + std::string(m_undoStack.GetRedoCommandName());
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, m_undoStack.CanRedo())) {
                m_undoStack.Redo();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Clear Undo History", nullptr, false, m_undoStack.GetUndoCount() > 0 || m_undoStack.GetRedoCount() > 0)) {
                m_undoStack.Clear();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Camera View (F)")) {
                m_camera.FocusOnTarget(glm::vec3(0.0f));
            }
            if (ImGui::MenuItem("Toggle Fullscreen", "F11", m_window.IsFullscreen())) {
                m_window.ToggleFullscreen();
            }
            if (ImGui::MenuItem("Reset Layout")) {
                m_rebuildLayout = true;
            }
            if (ImGui::BeginMenu("UI Scale / Zoom")) {
                auto applyScale = [this](float s) {
                    m_pendingFontScale = s;
                };
                if (ImGui::MenuItem("100% (Normal)", nullptr, m_uiScale == 1.0f))  applyScale(1.0f);
                if (ImGui::MenuItem("125% (Medium)", nullptr, m_uiScale == 1.25f)) applyScale(1.25f);
                if (ImGui::MenuItem("150% (Large)",  nullptr, m_uiScale == 1.5f))  applyScale(1.5f);
                if (ImGui::MenuItem("175% (X-Large)",nullptr, m_uiScale == 1.75f)) applyScale(1.75f);
                if (ImGui::MenuItem("200% (2x HiDPI)",nullptr, m_uiScale == 2.0f)) applyScale(2.0f);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Set Application Resolution")) {
                if (ImGui::MenuItem("1280 x 720 (720p HD)")) {
                    glfwSetWindowSize(m_window.GetNativeWindow(), 1280, 720);
                }
                if (ImGui::MenuItem("1600 x 900 (900p HD+)")) {
                    glfwSetWindowSize(m_window.GetNativeWindow(), 1600, 900);
                }
                if (ImGui::MenuItem("1920 x 1080 (1080p Full HD)")) {
                    glfwSetWindowSize(m_window.GetNativeWindow(), 1920, 1080);
                }
                if (ImGui::MenuItem("2560 x 1440 (1440p QHD)")) {
                    glfwSetWindowSize(m_window.GetNativeWindow(), 2560, 1440);
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
                    strncpy(m_gltfPathInput, selectedPath.c_str(), sizeof(m_gltfPathInput));
                    m_gltfPathInput[sizeof(m_gltfPathInput) - 1] = '\0';
                }
            }
            if (ImGui::Button("Load Model", ImVec2(120, 0))) {
                LoadGLTFModel(m_gltfPathInput);
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
            ImGui::Text("• Architecture: Two-Tiered Data-Oriented Index-Based Mesh Topology");
            ImGui::Separator();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMainMenuBar();
    }
}

static void RenderEngineLogConsole(bool* p_open = nullptr) {
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

struct PushConstants {
    glm::mat4 mvp;
    glm::mat4 model;
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f};
    int32_t useTexture = 0;
    float shininess = 32.0f;
    float specularStrength = 0.5f;
    float ambientStrength = 0.15f;
};

static std::vector<uint32_t> LoadSPIRV(const std::string& path) {
    std::string filename = path;
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = path.substr(lastSlash + 1);
    }

    std::vector<std::string> candidatePaths = {
        path,
        "shaders/compiled/" + filename,
        "../" + path,
        "../../" + path,
        "../../../" + path,
        "../shaders/compiled/" + filename,
        "../../shaders/compiled/" + filename,
        "../../../shaders/compiled/" + filename
    };

    for (const auto& candidate : candidatePaths) {
        std::ifstream file(candidate, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t fileSize = static_cast<size_t>(file.tellg());
            if (fileSize > 0) {
                std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
                file.seekg(0);
                file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
                LOG_INFO("Successfully loaded shader SPIR-V from: " + candidate);
                return buffer;
            }
        }
    }

    LOG_ERROR("Failed to open shader file across all candidate paths: " + path);
    throw std::runtime_error("Failed to open shader file: " + path);
}

EditorApp::EditorApp()
    : m_window(1280, 720, "Khepri Engine - [Mesh Generation & Editing Mode]") {

    m_context = std::make_unique<VulkanContext>(m_window.GetNativeWindow(), true);
    m_swapchain = std::make_unique<Swapchain>(*m_context, m_window.GetWidth(), m_window.GetHeight());
    m_descriptorAllocator = std::make_unique<DescriptorAllocator>(*m_context, 100);

    InitImGui();
    InitRenderResources();
    CreateRenderPipeline();

    // Init Viewport & Editor Panels
    m_viewportPanel = std::make_unique<ViewportPanel>(*m_context);
    m_viewportPanel->GetGizmo().SetUndoStack(&m_undoStack);
    m_sceneTreePanel = std::make_unique<SceneTreePanel>(*m_context);
    m_sceneTreePanel->SetUndoStack(&m_undoStack);
    m_timelinePanel = std::make_unique<TimelinePanel>();
    m_vulkanInspectorPanel = std::make_unique<VulkanInspectorPanel>(*m_context);

    // Initialize AssetManager, Thumbnail Cache & Node Graph Engine
    khepri::AssetManager::Instance().Initialize(*m_context);
    m_thumbnailCache = std::make_unique<khepri::ui::ThumbnailCache>(m_context.get());
    m_nodeGraphEditorPanel = std::make_unique<khepri::NodeGraphEditorPanel>(*m_context);
    m_nodeGraphEditorPanel->SetUndoStack(&m_undoStack);
    m_assetManagerPanel = std::make_unique<khepri::AssetManagerPanel>(m_thumbnailCache.get());

    // Connect Asset & Viewport callbacks
    m_assetManagerPanel->SetOpenModelCallback([this](const std::string& path) {
        OpenSceneModel(path);
    });
    m_assetManagerPanel->SetImportModelCallback([this](const std::string& path) {
        ImportModelIntoScene(path);
    });
    m_viewportPanel->SetImportModelCallback([this](const std::string& path) {
        ImportModelIntoScene(path);
    });
    m_viewportPanel->SetSelectNodeCallback([this](SceneNode* node) {
        if (m_sceneTreePanel) {
            m_sceneTreePanel->SetSelectedNode(node);
        }
    });
    m_sceneTreePanel->SetImportModelCallback([this](const std::string& path) {
        ImportModelIntoScene(path);
    });
    m_sceneTreePanel->SetOpenModelCallback([this](const std::string& path) {
        OpenSceneModel(path);
    });
    m_sceneTreePanel->SetFocusCameraCallback([this](const glm::vec3& targetPos) {
        m_camera.FocusOnTarget(targetPos, 4.0f);
    });

    // Centralized Undo/Redo state observer
    m_undoStack.SetChangeListener([this]() {
        if (m_sceneTreePanel) {
            m_sceneTreePanel->ValidateSelection(m_rootNode.get());
        }
        if (m_nodeGraphEditorPanel) {
            m_nodeGraphEditorPanel->ValidateTargetSceneNode(m_rootNode.get());
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

    // Register Viewport Texture for ImGui rendering (initial DS creation)
    m_viewportDS = ImGui_ImplVulkan_AddTexture(
        m_viewportPanel->GetSampler(),
        m_viewportPanel->GetColorImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    BuildSampleScene();

    LOG_INFO("EditorApp initialization complete.");
}

EditorApp::~EditorApp() {
    m_context->WaitIdle();

    m_defaultWhiteTexture.reset();

    // Clean up the viewport descriptor set
    if (m_viewportDS != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_viewportDS);
        m_viewportDS = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_textureDescriptorSetLayout) vkDestroyDescriptorSetLayout(m_context->GetDevice(), m_textureDescriptorSetLayout, nullptr);
    if (m_imguiPool)         vkDestroyDescriptorPool(m_context->GetDevice(), m_imguiPool, nullptr);
    if (m_commandPool)       vkDestroyCommandPool(m_context->GetDevice(), m_commandPool, nullptr);
    if (m_graphicsPipeline)  vkDestroyPipeline(m_context->GetDevice(), m_graphicsPipeline, nullptr);
    if (m_wireframePipeline) vkDestroyPipeline(m_context->GetDevice(), m_wireframePipeline, nullptr);
    if (m_pipelineLayout)    vkDestroyPipelineLayout(m_context->GetDevice(), m_pipelineLayout, nullptr);
}

void EditorApp::InitImGui() {
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
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    vkCreateDescriptorPool(m_context->GetDevice(), &poolInfo, nullptr, &m_imguiPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Detect OS DPI Content Scale
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(m_window.GetNativeWindow(), &xscale, &yscale);
    khepri::ui::Theme::SetContentScale(xscale);
    khepri::ui::Theme::SetUserScale(m_uiScale);

    // Load Typography & Apply Design System Theme
    khepri::ui::Theme::LoadFonts(io, khepri::ui::Theme::GetTotalScale(), "assets/fonts");
    khepri::ui::Theme::ApplyTheme(khepri::ui::Theme::GetTotalScale());

    ImGui_ImplGlfw_InitForVulkan(m_window.GetNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_context->GetInstance();
    initInfo.PhysicalDevice = m_context->GetPhysicalDevice();
    initInfo.Device = m_context->GetDevice();
    initInfo.QueueFamily = m_context->GetQueueFamilies().graphicsFamily.value();
    initInfo.Queue = m_context->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiPool;
    initInfo.MinImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
    initInfo.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    VkFormat colorFormat = m_swapchain->GetImageFormat();
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = renderingInfo;

    ImGui_ImplVulkan_Init(&initInfo);
}

void EditorApp::UpdateLightUBO() {
    if (!m_lightUBOBuffer || !m_rootNode) return;

    LightUBO ubo{};
    ubo.cameraPos = glm::vec4(m_camera.GetPosition(), 0.0f);

    std::vector<LightData> activeLights;

    std::function<void(SceneNode*, const glm::mat4&)> collectLights = [&](SceneNode* node, const glm::mat4& parentTransform) {
        if (!node || !node->visible) return;

        glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();

        if (node->lightComponent) {
            glm::vec3 worldPos = glm::vec3(worldTransform[3]);
            glm::mat3 rotMat = glm::mat3(worldTransform);
            glm::vec3 worldDir = rotMat * node->lightComponent->direction;

            if (activeLights.size() < MAX_LIGHTS) {
                activeLights.push_back(node->lightComponent->GetGPUData(worldPos, worldDir));
            }
        }

        for (const auto& child : node->GetChildren()) {
            collectLights(child.get(), worldTransform);
        }
    };

    collectLights(m_rootNode.get(), glm::mat4(1.0f));

    ubo.cameraPos.w = static_cast<float>(activeLights.size());
    for (size_t i = 0; i < activeLights.size(); ++i) {
        ubo.lights[i] = activeLights[i];
    }

    m_lightUBOBuffer->CopyToBuffer(&ubo, sizeof(LightUBO));
}

void EditorApp::InitRenderResources() {
    // 1. Texture Descriptor Set Layout
    if (!m_textureDescriptorSetLayout) {
        std::vector<VkDescriptorSetLayoutBinding> bindings(2);
        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptorLayoutInfo.pBindings = bindings.data();

        const VkResult res = vkCreateDescriptorSetLayout(m_context->GetDevice(), &descriptorLayoutInfo, nullptr, &m_textureDescriptorSetLayout);
        CHECK_VK_RESULT(res, "Failed to create texture descriptor set layout");
    }

    // 2. Light UBO Buffer
    if (!m_lightUBOBuffer) {
        m_lightUBOBuffer = std::make_unique<Buffer>(
            *m_context,
            sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
    }

    // 3. Default White Texture
    if (!m_defaultWhiteTexture) {
        m_defaultWhiteTexture = Texture::CreateWhiteTexture(*m_context, m_textureDescriptorSetLayout, *m_descriptorAllocator, m_lightUBOBuffer->GetBuffer());
    }

    // 4. Pipeline Layout
    if (!m_pipelineLayout) {
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &m_textureDescriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        const VkResult res = vkCreatePipelineLayout(m_context->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout);
        CHECK_VK_RESULT(res, "Failed to create pipeline layout");
    }

    // 5. Command Pool & Command Buffers
    if (!m_commandPool) {
        m_commandPool = m_context->CreateCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        m_commandBuffers = m_context->AllocateCommandBuffers(m_commandPool, Swapchain::MAX_FRAMES_IN_FLIGHT);
    }
}

void EditorApp::CreateRenderPipeline() {
    // Destroy previous graphics & wireframe pipelines if rebuilding for MSAA
    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context->GetDevice(), m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }
    if (m_wireframePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context->GetDevice(), m_wireframePipeline, nullptr);
        m_wireframePipeline = VK_NULL_HANDLE;
    }

    std::vector<uint32_t> vCode = LoadSPIRV("shaders/compiled/mesh.vert.spv");
    std::vector<uint32_t> fCode = LoadSPIRV("shaders/compiled/mesh.frag.spv");

    VkShaderModule vertModule = PipelineBuilder::CreateShaderModule(*m_context, vCode);
    VkShaderModule fragModule = PipelineBuilder::CreateShaderModule(*m_context, fCode);

    const std::vector<VkVertexInputAttributeDescription> attribs = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = static_cast<uint32_t>(offsetof(Vertex, position)) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = static_cast<uint32_t>(offsetof(Vertex, normal)) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = static_cast<uint32_t>(offsetof(Vertex, uv)) }
    };

    const VkSampleCountFlagBits msaaSamples = m_viewportPanel ? m_viewportPanel->GetMSAASamples() : VK_SAMPLE_COUNT_8_BIT;
    m_currentPipelineMSAASamples = msaaSamples;

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule)
           .SetVertexInput(Vertex::GetBindingDescriptions(), attribs)
           .SetColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
           .SetDepthFormat(VK_FORMAT_D32_SFLOAT)
           .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
           .SetMultisampling(msaaSamples, false)
           .EnableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    m_graphicsPipeline = builder.Build(*m_context, m_pipelineLayout);

    builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
    m_wireframePipeline = builder.Build(*m_context, m_pipelineLayout);

    vkDestroyShaderModule(m_context->GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->GetDevice(), fragModule, nullptr);
}

void EditorApp::BuildSampleScene() {
    m_rootNode = std::make_unique<SceneNode>("Scene Root");

    // Add default Sun / Directional light node (Unreal ALight/ADirectionalLight style)
    m_rootNode->AddChild(std::make_unique<DirectionalLightNode>("Sun / Main Light", glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-45.0f, 45.0f, 0.0f)));

    // Initialize Default object in scene
    auto demoNode = std::make_unique<SceneNode>("Demo Cube");
    demoNode->mesh = MeshComponent::CreateCube(*m_context, 1.0f);
    SceneNode* demoPtr = m_rootNode->AddChild(std::move(demoNode));

    if (m_nodeGraphEditorPanel) {
        // Bind demo mesh as target scene node and initialize graph
        m_nodeGraphEditorPanel->SetTargetSceneNode(demoPtr);
        auto graphMesh = m_nodeGraphEditorPanel->GetActiveOutputMesh();
        m_activeDisplayMesh = graphMesh ? graphMesh : demoPtr->mesh;
    } else {
        m_activeDisplayMesh = demoPtr->mesh;
    }

    // Sample animation clip — NOT playing on startup (user must press Play)
    auto clip = std::make_shared<AnimationClip>();
    clip->name = "Spin Animation";
    clip->duration = 4.0f;

    AnimationTrack track;
    track.targetNodeName = "Demo Cylinder";
    track.positionKeys = {
        {0.0f, glm::vec3(0.0f, 0.0f, 0.0f)},
        {2.0f, glm::vec3(0.0f, 1.5f, 0.0f)},
        {4.0f, glm::vec3(0.0f, 0.0f, 0.0f)}
    };
    track.rotationKeys = {
        {0.0f, glm::quat(glm::vec3(0, 0, 0))},
        {2.0f, glm::quat(glm::vec3(0, glm::radians(180.0f), 0))},
        {4.0f, glm::quat(glm::vec3(0, glm::radians(360.0f), 0))}
    };
    clip->tracks.push_back(track);

    m_timeline.SetClip(clip);
    // Animation starts PAUSED — user must press Play in the Timeline panel
    m_camera.FocusOnTarget(glm::vec3(0.0f), 3.5f);
}

// ---------------------------------------------------------------------------
// Recursive scene graph renderer
// ---------------------------------------------------------------------------
void EditorApp::DrawSceneNode(VkCommandBuffer cmd, SceneNode* node, const glm::mat4& parentTransform) {
    if (!node) return;

    // If this node (or its entire subtree) is invisible, skip it
    if (!node->visible) return;

    // Compose world transform from parent and this node's local transform
    glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();

    // Draw the node's mesh (if it has one), or light gizmo stub if it's a light node
    std::shared_ptr<MeshComponent> targetMesh = node->mesh;

    glm::mat4 renderTransform = worldTransform;

    if (targetMesh) {
        VkDescriptorSet textureDS = targetMesh->HasTexture() ?
            targetMesh->GetTexture()->GetDescriptorSet() :
            m_defaultWhiteTexture->GetDescriptorSet();

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                0, 1, &textureDS, 0, nullptr);

        PushConstants push{};
        push.model = renderTransform;
        push.mvp   = m_camera.GetViewProjectionMatrix() * renderTransform;
        push.baseColorFactor = targetMesh->GetBaseColorFactor();
        push.emissiveFactor  = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        push.useTexture = targetMesh->HasTexture() ? 1 : 0;
        push.shininess = 32.0f;
        push.specularStrength = 0.5f;
        push.ambientStrength = 0.15f;

        // 1. Shaded Solid Pass (if Off or Overlay)
        if (node->wireframeMode != WireframeMode::WireframeOnly) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
            vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &push);
            targetMesh->Draw(cmd);
        }

        // 2. Wireframe Pass (if Overlay or WireframeOnly)
        if (node->wireframeMode == WireframeMode::Overlay || node->wireframeMode == WireframeMode::WireframeOnly) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_wireframePipeline);
            PushConstants wirePush = push;
            if (node->wireframeMode == WireframeMode::Overlay) {
                // High-contrast bright cyan overlay lines over solid mesh for topology inspection
                wirePush.baseColorFactor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                wirePush.useTexture = 0;
            }
            vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &wirePush);
            targetMesh->Draw(cmd);
        }
    }

    // Recurse into children
    for (const auto& child : node->GetChildren()) {
        DrawSceneNode(cmd, child.get(), worldTransform);
    }
}

void EditorApp::RenderViewportOffscreen(VkCommandBuffer cmd) {
    UpdateLightUBO();
    // Guard: don't attempt to render if the framebuffer image view is null
    if (m_viewportPanel->GetColorImageView() == VK_NULL_HANDLE) return;

    // Rebuild pipelines if MSAA sample count changed
    if (m_currentPipelineMSAASamples != m_viewportPanel->GetMSAASamples()) {
        m_context->WaitIdle();
        CreateRenderPipeline();
    }

    m_viewportPanel->TransitionToColorAttachment(cmd);

    VkSampleCountFlagBits msaaSamples = m_viewportPanel->GetMSAASamples();

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    if (msaaSamples > VK_SAMPLE_COUNT_1_BIT) {
        colorAttachment.imageView = m_viewportPanel->GetMSAAColorImageView();
        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.resolveImageView = m_viewportPanel->GetColorImageView();
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
        colorAttachment.imageView = m_viewportPanel->GetColorImageView();
    }
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = (msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.12f, 0.14f, 0.18f, 1.0f };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = (msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? m_viewportPanel->GetMSAADepthImageView() : m_viewportPanel->GetDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {m_viewportPanel->GetWidth(), m_viewportPanel->GetHeight()} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = static_cast<float>(m_viewportPanel->GetWidth());
    viewport.height = static_cast<float>(m_viewportPanel->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { m_viewportPanel->GetWidth(), m_viewportPanel->GetHeight() };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    // Traverse the entire scene graph — each node renders with its own world transform
    if (m_rootNode) {
        DrawSceneNode(cmd, m_rootNode.get(), glm::mat4(1.0f));
    }

    vkCmdEndRendering(cmd);

    m_viewportPanel->TransitionToShaderRead(cmd);
}

void EditorApp::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // 1. Offscreen 3D Viewport Pass
    RenderViewportOffscreen(cmd);

    // 2. Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->GetImages()[imageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // 3. ImGui Swapchain Pass
    VkRenderingAttachmentInfo swapchainColorAttachment{};
    swapchainColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    swapchainColorAttachment.imageView = m_swapchain->GetImageViews()[imageIndex];
    swapchainColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchainColorAttachment.clearValue.color = { 0.05f, 0.05f, 0.05f, 1.0f };

    VkRenderingInfo imguiRenderingInfo{};
    imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    imguiRenderingInfo.renderArea = { {0, 0}, m_swapchain->GetExtent() };
    imguiRenderingInfo.layerCount = 1;
    imguiRenderingInfo.colorAttachmentCount = 1;
    imguiRenderingInfo.pColorAttachments = &swapchainColorAttachment;

    vkCmdBeginRendering(cmd, &imguiRenderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    // 4. Transition swapchain image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->GetImages()[imageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);
}

void EditorApp::Run() {
    uint32_t currentFrame = 0;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        // Check for minimization (0x0 framebuffer)
        int fbWidth = 0, fbHeight = 0;
        m_window.GetFramebufferSize(&fbWidth, &fbHeight);
        if (fbWidth == 0 || fbHeight == 0) {
            m_window.WaitEventsTimeout(0.05);
            continue;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update Animation System (only ticks when m_timeline.IsPlaying() == true)
        m_timeline.Update(deltaTime, m_rootNode.get());

        // Acquire Swapchain Image
        uint32_t imageIndex;
        VkResult acquireResult = m_swapchain->AcquireNextImage(&imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
            int curW = 0, curH = 0;
            m_window.GetFramebufferSize(&curW, &curH);
            if (curW > 0 && curH > 0) {
                m_swapchain->Recreate(curW, curH);
            }
            continue;
        }

        // Handle deferred typography / UI scale changes at safe point between frames
        if (m_pendingFontScale > 0.0f) {
            m_context->WaitIdle();
            m_uiScale = m_pendingFontScale;
            khepri::ui::Theme::SetUserScale(m_uiScale);
            ImGuiIO& io = ImGui::GetIO();
            khepri::ui::Theme::LoadFonts(io, khepri::ui::Theme::GetTotalScale(), "assets/fonts");
            khepri::ui::Theme::ApplyTheme(khepri::ui::Theme::GetTotalScale());
            m_pendingFontScale = 0.0f;
        }

        // Start ImGui Frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        m_oracleBridge.PollEvents(ImGui::GetIO(), m_window.GetWidth(), m_window.GetHeight());
        ImGui::NewFrame();

        ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        RenderMainMenuBar(dockspaceID);

        if (m_rebuildLayout) {
            m_rebuildLayout = false;
            ApplyDockLayout(dockspaceID);
        }

        // Render UI panels
        if (m_showViewport && m_viewportPanel) {
            m_viewportPanel->RenderUI(m_camera, m_viewportDS, deltaTime,
                                      m_sceneTreePanel->GetSelectedNode(), m_rootNode.get());
        }
        if (m_showSceneTree && m_sceneTreePanel) {
            const auto& selectedAsset = m_assetManagerPanel ? m_assetManagerPanel->GetSelectedPath() : std::filesystem::path();
            m_sceneTreePanel->RenderUI(m_rootNode.get(), m_activeDisplayMesh, selectedAsset,
                                       m_nodeGraphEditorPanel.get());
        }
        if (m_showTimeline && m_timelinePanel) {
            m_timelinePanel->RenderUI(m_timeline, m_sceneTreePanel->GetSelectedNode(), m_rootNode.get());
        }
        if (m_showVulkanInspector && m_vulkanInspectorPanel) {
            m_vulkanInspectorPanel->RenderUI(*m_swapchain);
        }
        if (m_showNodeGraph && m_nodeGraphEditorPanel) {
            m_nodeGraphEditorPanel->ValidateTargetSceneNode(m_rootNode.get());
            SceneNode* selected = m_sceneTreePanel ? m_sceneTreePanel->GetSelectedNode() : nullptr;
            if (m_nodeGraphEditorPanel->GetTargetSceneNode() != selected) {
                m_nodeGraphEditorPanel->SetTargetSceneNode(selected);
            }

            m_nodeGraphEditorPanel->RenderUI(m_activeDisplayMesh);
            
            // Synchronize active graph output mesh to target SceneNode (excluding lights)
            if (selected && selected->mesh && m_activeDisplayMesh && !selected->lightComponent) {
                selected->mesh = m_activeDisplayMesh;
            }
        }
        if (m_showAssetManager && m_assetManagerPanel) {
            m_assetManagerPanel->RenderUI(&m_showAssetManager);
        }
        if (m_showLogConsole) {
            RenderEngineLogConsole(&m_showLogConsole);
        }

        // Global Keyboard Shortcuts (Undo: Ctrl+Z, Redo: Ctrl+Y / Ctrl+Shift+Z, Fullscreen: F11 / Alt+Enter, Focus: F, Rename: F2)
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_F11, false) ||
                (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
                m_window.ToggleFullscreen();
            } else if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                if (m_undoStack.CanUndo()) {
                    m_undoStack.Undo();
                }
            } else if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
                       (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
                if (m_undoStack.CanRedo()) {
                    m_undoStack.Redo();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
                if (m_sceneTreePanel && m_sceneTreePanel->GetSelectedNode()) {
                    m_camera.FocusOnTarget(m_sceneTreePanel->GetSelectedNode()->position, 4.0f);
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
                if (m_sceneTreePanel && m_sceneTreePanel->GetSelectedNode()) {
                    m_sceneTreePanel->StartRenaming(m_sceneTreePanel->GetSelectedNode());
                }
            }
        }

        ImGui::Render();

        // Record Commands & Submit
        VkCommandBuffer cmd = m_commandBuffers[currentFrame];
        vkResetCommandBuffer(cmd, 0);
        RecordCommandBuffer(cmd, imageIndex);

        Khepri::QueueSubmitDescriptor submitDesc{};
        submitDesc.commandBuffer = cmd;
        submitDesc.waitSemaphore = m_swapchain->GetImageAvailableSemaphore(imageIndex);
        submitDesc.waitStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitDesc.signalSemaphore = m_swapchain->GetRenderFinishedSemaphore(imageIndex);
        submitDesc.signalStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        submitDesc.fence = m_swapchain->GetInFlightFence(currentFrame);

        VkResult submitResult = m_context->GetGraphicsQueue().Submit(submitDesc);
        if (submitResult != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer! VkResult = "
                      + std::to_string(static_cast<int>(submitResult)));
        }

        VkResult presentResult = m_swapchain->Present(imageIndex);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR
            || m_window.WasResized()) {
            m_window.ResetResizedFlag();
            int curW = 0, curH = 0;
            m_window.GetFramebufferSize(&curW, &curH);
            if (curW > 0 && curH > 0) {
                m_swapchain->Recreate(curW, curH);
            }
        }

        currentFrame = (currentFrame + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
    }
}

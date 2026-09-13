#pragma once

#include <volk.h>
#include <memory>
#include <functional>
#include <string>

// Forward declarations — EngineContext only holds references/pointers,
// no ownership. Consumers must include the relevant headers themselves.
class Window;
class Camera;
class SceneNode;
class MeshComponent;
class Timeline;
class VulkanContext;
class Swapchain;
class DescriptorAllocator;

namespace khepri {
class OracleBridge;
namespace core { class UndoStack; }
} // namespace khepri

namespace khepri {

/// Lightweight, non-owning bridge that gives ISubsystem implementations access
/// to engine services and shared state. Populated by EditorApp and passed into
/// ISubsystem::Initialize(). Subsystems store a pointer to EngineContext for
/// later access during Update() / RenderUI().
///
/// All fields are raw references/pointers — EngineContext does not own any of
/// the objects it points to; lifetime is managed by EditorApp.
struct EngineContext {
    // --- Core services ---
    Window*             window = nullptr;
    VulkanContext*      vulkanContext = nullptr;
    Swapchain*          swapchain = nullptr;
    DescriptorAllocator* descriptorAllocator = nullptr;
    khepri::OracleBridge* oracleBridge = nullptr;

    // --- Scene state ---
    Camera*                         camera = nullptr;
    std::shared_ptr<SceneNode>*     rootNode = nullptr;
    std::shared_ptr<MeshComponent>* activeDisplayMesh = nullptr;
    Timeline*                       timeline = nullptr;
    khepri::core::UndoStack*        undoStack = nullptr;

    // --- Vulkan render resources shared with the scene renderer ---
    // (descriptor set layouts needed by panels that render into the viewport)
    VkDescriptorSetLayout textureDescriptorSetLayout = VK_NULL_HANDLE;

    // --- Editor action callbacks ---
    // Subsystems call these instead of calling EditorApp methods directly,
    // keeping the subsystem/app interface unidirectional.
    std::function<void(const std::string&)> openSceneModel;
    std::function<void(const std::string&)> importModelIntoScene;
};

} // namespace khepri
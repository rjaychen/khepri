#pragma once

namespace khepri {

struct EngineContext;

/// Abstract base for all engine subsystems.
///
/// Subsystems are registered with a SubsystemManager and have their lifecycle
/// methods dispatched in registration order (Initialize, Update, RenderUI) and
/// in strict reverse (LIFO) order during Shutdown, guaranteeing that a subsystem
/// that depends on another is always shut down before its dependency.
class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    ISubsystem(const ISubsystem&) = delete;
    ISubsystem& operator=(const ISubsystem&) = delete;
    ISubsystem(ISubsystem&&) = delete;
    ISubsystem& operator=(ISubsystem&&) = delete;

    /// Called once after construction. Subsystem should acquire all Vulkan and
    /// engine resources it needs via the provided EngineContext.
    virtual void Initialize(EngineContext& context) = 0;

    /// Called once per frame before rendering. deltaTime is in seconds.
    virtual void Update([[maybe_unused]] float deltaTime) {}

    /// Called once per frame inside the active ImGui frame (between NewFrame and Render).
    virtual void RenderUI() {}

    /// Called once at shutdown, in reverse registration order. Subsystem must
    /// release all resources acquired during Initialize().
    virtual void Shutdown() = 0;

    /// Returns a stable human-readable name used in logs and diagnostics.
    [[nodiscard]] virtual const char* GetName() const noexcept = 0;

protected:
    ISubsystem() = default;
};

} // namespace khepri
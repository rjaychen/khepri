#pragma once

#include "ISubsystem.h"
#include <vector>
#include <memory>
#include <type_traits>
#include <string>

namespace khepri {

struct EngineContext;

/// Manages registration, lifecycle dispatch, and deterministic LIFO teardown
/// of all engine subsystems.
class SubsystemManager {
public:
    SubsystemManager() = default;
    ~SubsystemManager();

    SubsystemManager(const SubsystemManager&) = delete;
    SubsystemManager& operator=(const SubsystemManager&) = delete;
    SubsystemManager(SubsystemManager&&) noexcept = default;
    SubsystemManager& operator=(SubsystemManager&&) noexcept = default;

    /// Registers a new subsystem instance. Subsystems will be initialized in
    /// registration order and shut down in reverse (LIFO) order.
    template<typename T, typename... Args>
    T* AddSubsystem(Args&&... args) {
        static_assert(std::is_base_of_v<ISubsystem, T>, "T must inherit from ISubsystem");
        auto subsystem = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = subsystem.get();
        m_subsystems.emplace_back(std::move(subsystem));
        return rawPtr;
    }

    /// Finds a registered subsystem by type. Returns nullptr if not found.
    template<typename T>
    [[nodiscard]] T* GetSubsystem() const {
        static_assert(std::is_base_of_v<ISubsystem, T>, "T must inherit from ISubsystem");
        for (const auto& sub : m_subsystems) {
            if (auto casted = dynamic_cast<T*>(sub.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    /// Calls Initialize() on all registered subsystems in forward registration order.
    void InitializeAll(EngineContext& context);

    /// Calls Update(deltaTime) on all initialized subsystems in forward order.
    void UpdateAll(float deltaTime);

    /// Calls RenderUI() on all initialized subsystems in forward order.
    void RenderUIAll();

    /// Shuts down and releases all subsystems in strict reverse (LIFO) order.
    /// Safe to call multiple times (subsequent calls are no-ops).
    void ShutdownAll();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] size_t GetSubsystemCount() const noexcept { return m_subsystems.size(); }

private:
    std::vector<std::unique_ptr<ISubsystem>> m_subsystems;
    bool m_initialized = false;
};

} // namespace khepri
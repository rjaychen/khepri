#include "SubsystemManager.h"
#include "Logger.h"

namespace khepri {

SubsystemManager::~SubsystemManager() {
    ShutdownAll();
}

void SubsystemManager::InitializeAll(EngineContext& context) {
    if (m_initialized) {
        LOG_WARN("SubsystemManager::InitializeAll called but manager is already initialized.");
        return;
    }

    LOG_INFO("SubsystemManager: Initializing " + std::to_string(m_subsystems.size()) + " subsystems...");
    for (auto& subsystem : m_subsystems) {
        if (subsystem) {
            LOG_INFO(std::string("SubsystemManager: Initializing subsystem [") + subsystem->GetName() + "]");
            subsystem->Initialize(context);
        }
    }
    m_initialized = true;
    LOG_INFO("SubsystemManager: All subsystems initialized successfully.");
}

void SubsystemManager::UpdateAll(float deltaTime) {
    if (!m_initialized) return;

    for (auto& subsystem : m_subsystems) {
        if (subsystem) {
            subsystem->Update(deltaTime);
        }
    }
}

void SubsystemManager::RenderUIAll() {
    if (!m_initialized) return;

    for (auto& subsystem : m_subsystems) {
        if (subsystem) {
            subsystem->RenderUI();
        }
    }
}

void SubsystemManager::ShutdownAll() {
    if (m_subsystems.empty()) return;

    LOG_INFO("SubsystemManager: Shutting down subsystems in reverse (LIFO) order...");
    // Iterate in reverse (LIFO) order
    for (auto it = m_subsystems.rbegin(); it != m_subsystems.rend(); ++it) {
        if (*it) {
            LOG_INFO(std::string("SubsystemManager: Shutting down subsystem [") + (*it)->GetName() + "]");
            (*it)->Shutdown();
        }
    }
    // Destroy all subsystem unique_ptrs in reverse order
    while (!m_subsystems.empty()) {
        m_subsystems.pop_back();
    }

    m_initialized = false;
    LOG_INFO("SubsystemManager: All subsystems shut down and destroyed.");
}

} // namespace khepri
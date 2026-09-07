#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <functional>

namespace khepri::core {

enum class CommandId : uint32_t {
    Generic = 0,
    Compound = 1,
    Lambda = 2,
    Transform = 3,
    SceneHierarchy = 4,
    NodeGraph = 5,
    Keyframe = 6
};

/**
 * @brief Base interface for all invertible editor actions and state deltas.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Executes the command forward.
     */
    virtual void Execute() = 0;

    /**
     * @brief Inverts the command, reverting state to before Execute().
     */
    virtual void Undo() = 0;

    /**
     * @brief Re-applies the command after an Undo().
     * Defaults to calling Execute().
     */
    virtual void Redo() {
        Execute();
    }

    /**
     * @brief Human-readable name/description for UI display (e.g. "Translate Node", "Add Link").
     */
    [[nodiscard]] virtual std::string_view GetName() const noexcept = 0;

    /**
     * @brief Returns a fast type identifier for non-RTTI merging checks.
     */
    [[nodiscard]] virtual CommandId GetCommandId() const noexcept {
        return CommandId::Generic;
    }

    /**
     * @brief Attempts to merge a consecutive incoming command into this command.
     * Useful for continuous UI slider scrubbing, text typing, or canvas dragging.
     * @param other Pointer to the incoming command.
     * @return true if merged successfully; false otherwise.
     */
    [[nodiscard]] virtual bool MergeWith([[maybe_unused]] const ICommand* other) {
        return false;
    }

    /**
     * @brief Returns approximate heap/memory usage in bytes for cache tracking.
     */
    [[nodiscard]] virtual size_t GetApproximateMemoryUsage() const noexcept {
        return sizeof(*this);
    }
};

/**
 * @brief Compound command grouping multiple commands executed and undone as a single atomic unit.
 */
class CompoundCommand : public ICommand {
public:
    explicit CompoundCommand(std::string name = "Compound Action") noexcept
        : m_name(std::move(name)) {}

    [[nodiscard]] CommandId GetCommandId() const noexcept override {
        return CommandId::Compound;
    }

    void AddCommand(std::unique_ptr<ICommand> cmd) {
        if (cmd) {
            m_commands.push_back(std::move(cmd));
        }
    }

    void Execute() override {
        for (auto& cmd : m_commands) {
            cmd->Execute();
        }
    }

    void Undo() override {
        // Execute undo in reverse order to preserve dependencies
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->Undo();
        }
    }

    void Redo() override {
        for (auto& cmd : m_commands) {
            cmd->Redo();
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] bool IsEmpty() const noexcept {
        return m_commands.empty();
    }

    [[nodiscard]] size_t GetCommandCount() const noexcept {
        return m_commands.size();
    }

    [[nodiscard]] const std::vector<std::unique_ptr<ICommand>>& GetCommands() const noexcept {
        return m_commands;
    }

    [[nodiscard]] size_t GetApproximateMemoryUsage() const noexcept override {
        size_t total = sizeof(*this) + m_name.capacity();
        for (const auto& cmd : m_commands) {
            if (cmd) {
                total += cmd->GetApproximateMemoryUsage();
            }
        }
        return total;
    }

private:
    std::string m_name;
    std::vector<std::unique_ptr<ICommand>> m_commands;
};

/**
 * @brief Lightweight command constructed from functional closures.
 */
class LambdaCommand : public ICommand {
public:
    LambdaCommand(std::string name, std::function<void()> executeFn, std::function<void()> undoFn)
        : m_name(std::move(name)), m_executeFn(std::move(executeFn)), m_undoFn(std::move(undoFn)) {}

    void Execute() override {
        if (m_executeFn) m_executeFn();
    }

    void Undo() override {
        if (m_undoFn) m_undoFn();
    }

    [[nodiscard]] CommandId GetCommandId() const noexcept override {
        return CommandId::Lambda;
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    std::string m_name;
    std::function<void()> m_executeFn;
    std::function<void()> m_undoFn;
};

} // namespace khepri::core

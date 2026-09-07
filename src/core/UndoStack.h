#pragma once

#include "Command.h"
#include <deque>
#include <memory>
#include <functional>
#include <string_view>

namespace khepri::core {

/**
 * @brief Centralized Undo/Redo stack manager with transaction grouping and capacity limiting.
 */
class UndoStack {
public:
    explicit UndoStack(size_t maxHistory = 100) noexcept;
    ~UndoStack() noexcept = default;

    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;
    UndoStack(UndoStack&&) noexcept = default;
    UndoStack& operator=(UndoStack&&) noexcept = default;

    /**
     * @brief Pushes a command to the stack and immediately invokes Execute().
     * Clears the redo stack and trims old history if exceeding max capacity.
     */
    void PushAndExecute(std::unique_ptr<ICommand> cmd);

    /**
     * @brief Pushes an already-executed command to the undo stack.
     * Useful when live UI dragging already applied the mutation and only the final delta is committed.
     */
    void Push(std::unique_ptr<ICommand> cmd);

    /**
     * @brief Inverts the most recent command on the undo stack and moves it to the redo stack.
     * @return true if an action was undone; false if undo stack was empty.
     */
    bool Undo();

    /**
     * @brief Re-applies the most recently undone command from the redo stack and moves it to the undo stack.
     * @return true if an action was redone; false if redo stack was empty.
     */
    bool Redo();

    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;

    [[nodiscard]] std::string_view GetUndoCommandName() const noexcept;
    [[nodiscard]] std::string_view GetRedoCommandName() const noexcept;

    [[nodiscard]] size_t GetUndoCount() const noexcept { return m_undoStack.size(); }
    [[nodiscard]] size_t GetRedoCount() const noexcept { return m_redoStack.size(); }
    [[nodiscard]] size_t GetMaxHistory() const noexcept { return m_maxHistory; }
    void SetMaxHistory(size_t maxHistory) noexcept;

    // --- Clean / Save State API ---
    /**
     * @brief Marks the current stack position as the saved (clean) state.
     */
    void MarkClean() noexcept;

    /**
     * @brief Returns true if the stack is currently at the saved clean state.
     */
    [[nodiscard]] bool IsClean() const noexcept { return m_currentIndex == m_cleanIndex; }

    /**
     * @brief Returns true if the document has unsaved modifications.
     */
    [[nodiscard]] bool IsDirty() const noexcept { return !IsClean(); }

    [[nodiscard]] int64_t GetCleanIndex() const noexcept { return m_cleanIndex; }
    [[nodiscard]] int64_t GetCurrentIndex() const noexcept { return m_currentIndex; }

    /**
     * @brief Clears both undo and redo stacks and cancels any active transaction.
     */
    void Clear() noexcept;

    // --- Transaction API ---
    /**
     * @brief Begins a compound transaction. All subsequent commands pushed will be
     * accumulated into an atomic CompoundCommand until EndTransaction() is called.
     * Supports nested transaction calls.
     */
    void BeginTransaction(std::string_view name);

    /**
     * @brief Closes the active transaction and commits the CompoundCommand to the undo stack.
     */
    void EndTransaction();

    /**
     * @brief Cancels the active transaction, undoing any commands executed within it and discarding the group.
     */
    void CancelTransaction();

    [[nodiscard]] bool IsInTransaction() const noexcept { return m_transactionDepth > 0; }

    /**
     * @brief Registers a callback to be notified whenever undo/redo state changes.
     */
    void SetChangeListener(std::function<void()> listener) {
        m_changeListener = std::move(listener);
    }

private:
    void NotifyChanged() const;
    void TrimHistory();

    size_t m_maxHistory{100};
    std::deque<std::unique_ptr<ICommand>> m_undoStack;
    std::deque<std::unique_ptr<ICommand>> m_redoStack;

    // Save point tracking
    int64_t m_cleanIndex{0};
    int64_t m_currentIndex{0};

    // Transaction state
    size_t m_transactionDepth{0};
    std::unique_ptr<CompoundCommand> m_activeTransaction;

    std::function<void()> m_changeListener;
};

/**
 * @brief RAII transaction guard that automatically commits or rolls back a transaction.
 */
class ScopedTransaction {
public:
    explicit ScopedTransaction(UndoStack& stack, std::string_view name) noexcept
        : m_stack(&stack), m_active(true) {
        m_stack->BeginTransaction(name);
    }

    ~ScopedTransaction() {
        if (m_active && m_stack) {
            m_stack->EndTransaction();
        }
    }

    void Commit() {
        if (m_active && m_stack) {
            m_stack->EndTransaction();
            m_active = false;
        }
    }

    void Cancel() {
        if (m_active && m_stack) {
            m_stack->CancelTransaction();
            m_active = false;
        }
    }

    ScopedTransaction(const ScopedTransaction&) = delete;
    ScopedTransaction& operator=(const ScopedTransaction&) = delete;

    ScopedTransaction(ScopedTransaction&& other) noexcept
        : m_stack(other.m_stack), m_active(other.m_active) {
        other.m_stack = nullptr;
        other.m_active = false;
    }

    ScopedTransaction& operator=(ScopedTransaction&& other) noexcept {
        if (this != &other) {
            if (m_active && m_stack) {
                m_stack->EndTransaction();
            }
            m_stack = other.m_stack;
            m_active = other.m_active;
            other.m_stack = nullptr;
            other.m_active = false;
        }
        return *this;
    }

private:
    UndoStack* m_stack{nullptr};
    bool m_active{false};
};

} // namespace khepri::core

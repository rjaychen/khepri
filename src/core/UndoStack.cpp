#include "UndoStack.h"
#include <algorithm>

namespace khepri::core {

UndoStack::UndoStack(size_t maxHistory) noexcept
    : m_maxHistory(std::max<size_t>(1, maxHistory)) {}

void UndoStack::PushAndExecute(std::unique_ptr<ICommand> cmd) {
    if (!cmd) return;

    if (m_transactionDepth > 0 && m_activeTransaction) {
        cmd->Execute();
        m_activeTransaction->AddCommand(std::move(cmd));
        NotifyChanged();
        return;
    }

    if (!m_undoStack.empty() && m_undoStack.back()->MergeWith(cmd.get())) {
        m_redoStack.clear();
        NotifyChanged();
        return;
    }

    cmd->Execute();
    m_redoStack.clear();
    m_undoStack.push_back(std::move(cmd));
    ++m_currentIndex;
    TrimHistory();
    NotifyChanged();
}

void UndoStack::Push(std::unique_ptr<ICommand> cmd) {
    if (!cmd) return;

    if (m_transactionDepth > 0 && m_activeTransaction) {
        m_activeTransaction->AddCommand(std::move(cmd));
        NotifyChanged();
        return;
    }

    if (!m_undoStack.empty() && m_undoStack.back()->MergeWith(cmd.get())) {
        m_redoStack.clear();
        NotifyChanged();
        return;
    }

    m_redoStack.clear();
    m_undoStack.push_back(std::move(cmd));
    ++m_currentIndex;
    TrimHistory();
    NotifyChanged();
}

bool UndoStack::Undo() {
    if (m_undoStack.empty() || m_transactionDepth > 0) {
        return false;
    }

    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    cmd->Undo();
    m_redoStack.push_back(std::move(cmd));
    --m_currentIndex;

    NotifyChanged();
    return true;
}

bool UndoStack::Redo() {
    if (m_redoStack.empty() || m_transactionDepth > 0) {
        return false;
    }

    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    cmd->Redo();
    m_undoStack.push_back(std::move(cmd));
    ++m_currentIndex;

    NotifyChanged();
    return true;
}

bool UndoStack::CanUndo() const noexcept {
    return !m_undoStack.empty() && m_transactionDepth == 0;
}

bool UndoStack::CanRedo() const noexcept {
    return !m_redoStack.empty() && m_transactionDepth == 0;
}

std::string_view UndoStack::GetUndoCommandName() const noexcept {
    if (CanUndo()) {
        return m_undoStack.back()->GetName();
    }
    return {};
}

std::string_view UndoStack::GetRedoCommandName() const noexcept {
    if (CanRedo()) {
        return m_redoStack.back()->GetName();
    }
    return {};
}

void UndoStack::SetMaxHistory(size_t maxHistory) noexcept {
    m_maxHistory = std::max<size_t>(1, maxHistory);
    TrimHistory();
}

void UndoStack::MarkClean() noexcept {
    m_cleanIndex = m_currentIndex;
    NotifyChanged();
}

void UndoStack::Clear() noexcept {
    m_undoStack.clear();
    m_redoStack.clear();
    m_activeTransaction.reset();
    m_transactionDepth = 0;
    m_currentIndex = 0;
    m_cleanIndex = 0;
    NotifyChanged();
}

void UndoStack::BeginTransaction(std::string_view name) {
    if (m_transactionDepth == 0) {
        m_activeTransaction = std::make_unique<CompoundCommand>(std::string(name));
    }
    ++m_transactionDepth;
}

void UndoStack::EndTransaction() {
    if (m_transactionDepth == 0) return;

    --m_transactionDepth;
    if (m_transactionDepth == 0 && m_activeTransaction) {
        if (!m_activeTransaction->IsEmpty()) {
            m_redoStack.clear();
            m_undoStack.push_back(std::move(m_activeTransaction));
            ++m_currentIndex;
            TrimHistory();
        }
        m_activeTransaction.reset();
        NotifyChanged();
    }
}

void UndoStack::CancelTransaction() {
    if (m_activeTransaction) {
        m_activeTransaction->Undo();
        m_activeTransaction.reset();
    }
    m_transactionDepth = 0;
    NotifyChanged();
}

void UndoStack::NotifyChanged() const {
    if (m_changeListener) {
        m_changeListener();
    }
}

void UndoStack::TrimHistory() {
    while (m_undoStack.size() > m_maxHistory) {
        m_undoStack.pop_front();
    }
}

} // namespace khepri::core

#pragma once

#include "../core/Command.h"
#include "SceneNode.h"
#include <memory>
#include <string>

namespace khepri::scene {

/**
 * @brief Command to add a child node to a parent SceneNode (or root).
 */
class AddChildNodeCommand : public core::ICommand {
public:
    AddChildNodeCommand(SceneNode* parent, std::unique_ptr<SceneNode> child, std::string name = "Add Scene Node")
        : m_parent(parent), m_child(std::move(child)), m_name(std::move(name)) {
        if (m_child) {
            m_childPtr = m_child.get();
        }
    }

    void Execute() override {
        if (!m_parent || !m_child) return;
        m_childPtr = m_parent->AddChild(std::move(m_child));
    }

    void Undo() override {
        if (!m_parent || !m_childPtr) return;
        m_child = m_parent->DetachChild(m_childPtr);
    }

    void Redo() override {
        if (!m_parent || !m_child) return;
        m_childPtr = m_parent->AddChild(std::move(m_child));
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] SceneNode* GetCreatedNode() const noexcept {
        return m_childPtr;
    }

private:
    SceneNode* m_parent{nullptr};
    std::unique_ptr<SceneNode> m_child;
    SceneNode* m_childPtr{nullptr};
    std::string m_name;
};

/**
 * @brief Command to remove and detach a child node from its parent.
 */
class RemoveChildNodeCommand : public core::ICommand {
public:
    RemoveChildNodeCommand(SceneNode* parent, SceneNode* child, std::string name = "Delete Scene Node")
        : m_parent(parent), m_childPtr(child), m_name(std::move(name)) {}

    void Execute() override {
        if (!m_parent || !m_childPtr) return;
        m_detachedChild = m_parent->DetachChild(m_childPtr);
    }

    void Undo() override {
        if (!m_parent || !m_detachedChild) return;
        m_childPtr = m_parent->AddChild(std::move(m_detachedChild));
    }

    void Redo() override {
        Execute();
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] SceneNode* GetTargetNode() const noexcept {
        return m_childPtr;
    }

private:
    SceneNode* m_parent{nullptr};
    SceneNode* m_childPtr{nullptr};
    std::unique_ptr<SceneNode> m_detachedChild;
    std::string m_name;
};

/**
 * @brief Command to reparent a SceneNode from one parent to another.
 */
class ReparentNodeCommand : public core::ICommand {
public:
    ReparentNodeCommand(SceneNode* node, SceneNode* newParent, std::string name = "Reparent Node")
        : m_node(node), m_oldParent(node ? node->GetParent() : nullptr), m_newParent(newParent),
          m_name(std::move(name)) {}

    void Execute() override {
        if (!m_node || !m_oldParent || !m_newParent) return;
        auto detached = m_oldParent->DetachChild(m_node);
        if (detached) {
            m_node = m_newParent->AddChild(std::move(detached));
        }
    }

    void Undo() override {
        if (!m_node || !m_oldParent || !m_newParent) return;
        auto detached = m_newParent->DetachChild(m_node);
        if (detached) {
            m_node = m_oldParent->AddChild(std::move(detached));
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    SceneNode* m_node{nullptr};
    SceneNode* m_oldParent{nullptr};
    SceneNode* m_newParent{nullptr};
    std::string m_name;
};

/**
 * @brief Command to rename a SceneNode.
 */
class RenameNodeCommand : public core::ICommand {
public:
    RenameNodeCommand(SceneNode* node, std::string newName, std::string name = "Rename Node")
        : m_node(node), m_oldName(node ? node->name : ""), m_newName(std::move(newName)),
          m_name(std::move(name)) {}

    void Execute() override {
        if (!m_node) return;
        m_node->name = m_newName;
    }

    void Undo() override {
        if (!m_node) return;
        m_node->name = m_oldName;
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] bool MergeWith(const core::ICommand* other) override {
        auto otherRename = dynamic_cast<const RenameNodeCommand*>(other);
        if (otherRename && otherRename->m_node == m_node) {
            m_newName = otherRename->m_newName;
            Execute();
            return true;
        }
        return false;
    }

private:
    SceneNode* m_node{nullptr};
    std::string m_oldName;
    std::string m_newName;
    std::string m_name;
};

/**
 * @brief Command to toggle visibility on a SceneNode.
 */
class SetNodeVisibilityCommand : public core::ICommand {
public:
    SetNodeVisibilityCommand(SceneNode* node, bool visible, std::string name = "Toggle Visibility")
        : m_node(node), m_oldVisibility(node ? node->visible : true), m_newVisibility(visible),
          m_name(std::move(name)) {}

    void Execute() override {
        if (!m_node) return;
        m_node->visible = m_newVisibility;
    }

    void Undo() override {
        if (!m_node) return;
        m_node->visible = m_oldVisibility;
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    SceneNode* m_node{nullptr};
    bool m_oldVisibility{true};
    bool m_newVisibility{true};
    std::string m_name;
};

} // namespace khepri::scene

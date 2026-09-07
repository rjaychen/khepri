#pragma once

#include "../core/Command.h"
#include "SceneNode.h"
#include <glm/glm.hpp>
#include <string>

namespace khepri::scene {

/**
 * @brief Command encapsulating a transform delta (position, rotation degrees, scale) on a SceneNode.
 */
class TransformCommand : public core::ICommand {
public:
    TransformCommand(
        SceneNode* node,
        const glm::vec3& oldPosition,
        const glm::vec3& oldRotation,
        const glm::vec3& oldScale,
        const glm::vec3& newPosition,
        const glm::vec3& newRotation,
        const glm::vec3& newScale,
        std::string name = "Transform Node"
    ) : m_node(node),
        m_oldPosition(oldPosition), m_oldRotation(oldRotation), m_oldScale(oldScale),
        m_newPosition(newPosition), m_newRotation(newRotation), m_newScale(newScale),
        m_name(std::move(name)) {}

    void Execute() override {
        if (!m_node) return;
        m_node->position = m_newPosition;
        m_node->rotationDegrees = m_newRotation;
        m_node->scale = m_newScale;
        m_node->SyncPropertiesToTransform();
    }

    void Undo() override {
        if (!m_node) return;
        m_node->position = m_oldPosition;
        m_node->rotationDegrees = m_oldRotation;
        m_node->scale = m_oldScale;
        m_node->SyncPropertiesToTransform();
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] bool MergeWith(const core::ICommand* other) override {
        auto otherTransform = dynamic_cast<const TransformCommand*>(other);
        if (otherTransform && otherTransform->m_node == m_node) {
            m_newPosition = otherTransform->m_newPosition;
            m_newRotation = otherTransform->m_newRotation;
            m_newScale = otherTransform->m_newScale;
            Execute();
            return true;
        }
        return false;
    }

    [[nodiscard]] SceneNode* GetNode() const noexcept { return m_node; }
    [[nodiscard]] const glm::vec3& GetOldPosition() const noexcept { return m_oldPosition; }
    [[nodiscard]] const glm::vec3& GetNewPosition() const noexcept { return m_newPosition; }
    [[nodiscard]] const glm::vec3& GetOldRotation() const noexcept { return m_oldRotation; }
    [[nodiscard]] const glm::vec3& GetNewRotation() const noexcept { return m_newRotation; }
    [[nodiscard]] const glm::vec3& GetOldScale() const noexcept { return m_oldScale; }
    [[nodiscard]] const glm::vec3& GetNewScale() const noexcept { return m_newScale; }

private:
    SceneNode* m_node{nullptr};
    glm::vec3 m_oldPosition{0.0f};
    glm::vec3 m_oldRotation{0.0f};
    glm::vec3 m_oldScale{1.0f};
    glm::vec3 m_newPosition{0.0f};
    glm::vec3 m_newRotation{0.0f};
    glm::vec3 m_newScale{1.0f};
    std::string m_name;
};

} // namespace khepri::scene

#pragma once

#include "../core/Command.h"
#include "NodeGraph.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace khepri::graph {

/**
 * @brief Command to add a node to the NodeGraph.
 */
class AddNodeCommand : public core::ICommand {
public:
    AddNodeCommand(
        NodeGraph* graph,
        std::shared_ptr<GraphNode> node,
        std::unordered_map<uint32_t, ImVec2>* positionsMap = nullptr,
        ImVec2 position = ImVec2(0.0f, 0.0f),
        std::string name = "Add Graph Node"
    ) : m_graph(graph), m_node(std::move(node)), m_positionsMap(positionsMap), m_position(position), m_name(std::move(name)) {}

    void Execute() override {
        if (!m_graph || !m_node) return;
        m_graph->RestoreNode(m_node);
        if (m_positionsMap) {
            (*m_positionsMap)[m_node->GetId()] = m_position;
        }
    }

    void Undo() override {
        if (!m_graph || !m_node) return;
        if (m_positionsMap) {
            m_positionsMap->erase(m_node->GetId());
        }
        m_graph->RemoveNode(m_node->GetId());
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] std::shared_ptr<GraphNode> GetNode() const noexcept {
        return m_node;
    }

private:
    NodeGraph* m_graph{nullptr};
    std::shared_ptr<GraphNode> m_node;
    std::unordered_map<uint32_t, ImVec2>* m_positionsMap{nullptr};
    ImVec2 m_position{0.0f, 0.0f};
    std::string m_name;
};

/**
 * @brief Command to delete a node and preserve all connected links for full undo restoration.
 */
class DeleteNodeCommand : public core::ICommand {
public:
    struct SavedConnection {
        uint32_t outputPinId{0};
        uint32_t inputPinId{0};
    };

    DeleteNodeCommand(
        NodeGraph* graph,
        std::shared_ptr<GraphNode> node,
        std::unordered_map<uint32_t, ImVec2>* positionsMap = nullptr,
        std::string name = "Delete Graph Node"
    ) : m_graph(graph), m_node(std::move(node)), m_positionsMap(positionsMap), m_name(std::move(name)) {
        if (m_positionsMap && m_node) {
            auto it = m_positionsMap->find(m_node->GetId());
            if (it != m_positionsMap->end()) {
                m_savedPosition = it->second;
                m_hasSavedPosition = true;
            }
        }
        CaptureConnections();
    }

    void Execute() override {
        if (!m_graph || !m_node) return;
        CaptureConnections();
        if (m_positionsMap) {
            m_positionsMap->erase(m_node->GetId());
        }
        m_graph->RemoveNode(m_node->GetId());
    }

    void Undo() override {
        if (!m_graph || !m_node) return;
        m_graph->RestoreNode(m_node);
        if (m_positionsMap && m_hasSavedPosition) {
            (*m_positionsMap)[m_node->GetId()] = m_savedPosition;
        }
        for (const auto& conn : m_savedConnections) {
            (void)m_graph->Connect(conn.outputPinId, conn.inputPinId);
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] std::shared_ptr<GraphNode> GetNode() const noexcept {
        return m_node;
    }

private:
    void CaptureConnections() {
        if (!m_graph || !m_node) return;
        m_savedConnections.clear();

        // 1. Incoming connections to this node's input pins
        for (const auto& pin : m_node->GetInputs()) {
            if (pin.connectedPinId != 0) {
                m_savedConnections.push_back({pin.connectedPinId, pin.id});
            }
        }

        // 2. Outgoing connections from this node's output pins to other nodes' input pins
        for (const auto& [otherId, otherNode] : m_graph->GetNodes()) {
            if (otherId == m_node->GetId() || !otherNode) continue;
            for (const auto& pin : otherNode->GetInputs()) {
                if (pin.connectedPinId != 0) {
                    for (const auto& outPin : m_node->GetOutputs()) {
                        if (pin.connectedPinId == outPin.id) {
                            m_savedConnections.push_back({outPin.id, pin.id});
                        }
                    }
                }
            }
        }
    }

    NodeGraph* m_graph{nullptr};
    std::shared_ptr<GraphNode> m_node;
    std::unordered_map<uint32_t, ImVec2>* m_positionsMap{nullptr};
    ImVec2 m_savedPosition{0.0f, 0.0f};
    bool m_hasSavedPosition{false};
    std::vector<SavedConnection> m_savedConnections;
    std::string m_name;
};

/**
 * @brief Command to connect an output pin to an input pin.
 */
class ConnectPinsCommand : public core::ICommand {
public:
    ConnectPinsCommand(NodeGraph* graph, uint32_t outputPinId, uint32_t inputPinId, std::string name = "Connect Pins")
        : m_graph(graph), m_outputPinId(outputPinId), m_inputPinId(inputPinId), m_name(std::move(name)) {
        if (m_graph) {
            auto inPin = m_graph->FindPin(m_inputPinId);
            if (inPin && inPin->connectedPinId != 0) {
                m_previousOutputPinId = inPin->connectedPinId;
            }
        }
    }

    void Execute() override {
        if (!m_graph) return;
        auto inPin = m_graph->FindPin(m_inputPinId);
        if (inPin && inPin->connectedPinId != 0) {
            m_previousOutputPinId = inPin->connectedPinId;
        }
        (void)m_graph->Connect(m_outputPinId, m_inputPinId);
    }

    void Undo() override {
        if (!m_graph) return;
        m_graph->Disconnect(m_inputPinId);
        if (m_previousOutputPinId != 0) {
            (void)m_graph->Connect(m_previousOutputPinId, m_inputPinId);
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    NodeGraph* m_graph{nullptr};
    uint32_t m_outputPinId{0};
    uint32_t m_inputPinId{0};
    uint32_t m_previousOutputPinId{0};
    std::string m_name;
};

/**
 * @brief Command to disconnect an input pin.
 */
class DisconnectPinsCommand : public core::ICommand {
public:
    DisconnectPinsCommand(NodeGraph* graph, uint32_t inputPinId, std::string name = "Disconnect Pin")
        : m_graph(graph), m_inputPinId(inputPinId), m_name(std::move(name)) {
        if (m_graph) {
            auto inPin = m_graph->FindPin(m_inputPinId);
            if (inPin) {
                m_previousOutputPinId = inPin->connectedPinId;
            }
        }
    }

    void Execute() override {
        if (!m_graph) return;
        auto inPin = m_graph->FindPin(m_inputPinId);
        if (inPin) {
            m_previousOutputPinId = inPin->connectedPinId;
        }
        m_graph->Disconnect(m_inputPinId);
    }

    void Undo() override {
        if (!m_graph || m_previousOutputPinId == 0) return;
        (void)m_graph->Connect(m_previousOutputPinId, m_inputPinId);
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    NodeGraph* m_graph{nullptr};
    uint32_t m_inputPinId{0};
    uint32_t m_previousOutputPinId{0};
    std::string m_name;
};

/**
 * @brief Command to mutate a pin's value.
 */
class ChangePinValueCommand : public core::ICommand {
public:
    ChangePinValueCommand(NodeGraph* graph, uint32_t pinId, PinValue oldValue, PinValue newValue, std::string name = "Change Parameter")
        : m_graph(graph), m_pinId(pinId), m_oldValue(std::move(oldValue)), m_newValue(std::move(newValue)), m_name(std::move(name)) {}

    void Execute() override {
        if (!m_graph) return;
        auto pin = m_graph->FindPin(m_pinId);
        if (pin) {
            pin->value = m_newValue;
            auto node = m_graph->GetNode(pin->nodeId);
            if (node) node->MarkDirty();
        }
    }

    void Undo() override {
        if (!m_graph) return;
        auto pin = m_graph->FindPin(m_pinId);
        if (pin) {
            pin->value = m_oldValue;
            auto node = m_graph->GetNode(pin->nodeId);
            if (node) node->MarkDirty();
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] bool MergeWith(const core::ICommand* other) override {
        auto otherPin = dynamic_cast<const ChangePinValueCommand*>(other);
        if (otherPin && otherPin->m_graph == m_graph && otherPin->m_pinId == m_pinId) {
            m_newValue = otherPin->m_newValue;
            Execute();
            return true;
        }
        return false;
    }

private:
    NodeGraph* m_graph{nullptr};
    uint32_t m_pinId{0};
    PinValue m_oldValue;
    PinValue m_newValue;
    std::string m_name;
};

/**
 * @brief Command to translate a set of nodes in editor canvas space.
 */
class MoveNodesCommand : public core::ICommand {
public:
    struct NodePositionRecord {
        uint32_t nodeId{0};
        glm::vec2 oldPos{0.0f};
        glm::vec2 newPos{0.0f};
    };

    MoveNodesCommand(
        std::unordered_map<uint32_t, ImVec2>* nodePositionsMap,
        std::vector<NodePositionRecord> positions,
        std::string name = "Move Nodes"
    ) : m_nodePositionsMap(nodePositionsMap), m_positions(std::move(positions)), m_name(std::move(name)) {}

    void Execute() override {
        if (!m_nodePositionsMap) return;
        for (const auto& record : m_positions) {
            (*m_nodePositionsMap)[record.nodeId] = ImVec2(record.newPos.x, record.newPos.y);
        }
    }

    void Undo() override {
        if (!m_nodePositionsMap) return;
        for (const auto& record : m_positions) {
            (*m_nodePositionsMap)[record.nodeId] = ImVec2(record.oldPos.x, record.oldPos.y);
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

    [[nodiscard]] bool MergeWith(const core::ICommand* other) override {
        auto otherMove = dynamic_cast<const MoveNodesCommand*>(other);
        if (otherMove && otherMove->m_nodePositionsMap == m_nodePositionsMap &&
            otherMove->m_positions.size() == m_positions.size()) {
            for (size_t i = 0; i < m_positions.size(); ++i) {
                if (m_positions[i].nodeId == otherMove->m_positions[i].nodeId) {
                    m_positions[i].newPos = otherMove->m_positions[i].newPos;
                }
            }
            Execute();
            return true;
        }
        return false;
    }

private:
    std::unordered_map<uint32_t, ImVec2>* m_nodePositionsMap{nullptr};
    std::vector<NodePositionRecord> m_positions;
    std::string m_name;
};

} // namespace khepri::graph

#include "NodeGraph.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <utility>

namespace khepri::graph {

uint32_t GraphNode::s_nextPinId = 1;

GraphNode::GraphNode(uint32_t id, std::string name, NodeDomain domain) noexcept
    : m_id(id), m_name(std::move(name)), m_domain(domain) {}

void GraphNode::MarkDirty() noexcept {
    m_isDirty = true;
    if (m_graph) {
        m_graph->MarkNodeDirty(m_id);
    }
}

GraphPin& GraphNode::AddInput(const std::string& name, PinType type, PinValue defaultValue) {
    GraphPin pin;
    pin.id = s_nextPinId++;
    pin.nodeId = m_id;
    pin.name = name;
    pin.type = type;
    pin.direction = PinDirection::Input;
    pin.value = std::move(defaultValue);
    m_inputs.push_back(pin);
    if (m_graph) {
        m_graph->RegisterNodePins(*this);
    }
    return m_inputs.back();
}

GraphPin& GraphNode::AddOutput(const std::string& name, PinType type) {
    GraphPin pin;
    pin.id = s_nextPinId++;
    pin.nodeId = m_id;
    pin.name = name;
    pin.type = type;
    pin.direction = PinDirection::Output;
    m_outputs.push_back(pin);
    if (m_graph) {
        m_graph->RegisterNodePins(*this);
    }
    return m_outputs.back();
}

GraphPin* GraphNode::FindInput(const std::string& name) noexcept {
    for (auto& pin : m_inputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

const GraphPin* GraphNode::FindInput(const std::string& name) const noexcept {
    for (const auto& pin : m_inputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

GraphPin* GraphNode::FindOutput(const std::string& name) noexcept {
    for (auto& pin : m_outputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

const GraphPin* GraphNode::FindOutput(const std::string& name) const noexcept {
    for (const auto& pin : m_outputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

void NodeGraph::RegisterNodePins(const GraphNode& node) {
    const auto& inputs = node.GetInputs();
    for (size_t i = 0; i < inputs.size(); ++i) {
        m_pinMap[inputs[i].id] = PinRef{node.GetId(), PinDirection::Input, i};
    }
    const auto& outputs = node.GetOutputs();
    for (size_t i = 0; i < outputs.size(); ++i) {
        m_pinMap[outputs[i].id] = PinRef{node.GetId(), PinDirection::Output, i};
    }
}

GraphPin* NodeGraph::FindPin(uint32_t pinId) noexcept {
    auto it = m_pinMap.find(pinId);
    if (it == m_pinMap.end()) return nullptr;

    auto nodeIt = m_nodes.find(it->second.nodeId);
    if (nodeIt == m_nodes.end() || !nodeIt->second) return nullptr;

    auto& node = nodeIt->second;
    if (it->second.direction == PinDirection::Input) {
        auto& inputs = node->GetInputs();
        if (it->second.pinIndex < inputs.size()) return &inputs[it->second.pinIndex];
    } else {
        auto& outputs = node->GetOutputs();
        if (it->second.pinIndex < outputs.size()) return &outputs[it->second.pinIndex];
    }
    return nullptr;
}

const GraphPin* NodeGraph::FindPin(uint32_t pinId) const noexcept {
    auto it = m_pinMap.find(pinId);
    if (it == m_pinMap.end()) return nullptr;

    auto nodeIt = m_nodes.find(it->second.nodeId);
    if (nodeIt == m_nodes.end() || !nodeIt->second) return nullptr;

    const auto& node = nodeIt->second;
    if (it->second.direction == PinDirection::Input) {
        const auto& inputs = node->GetInputs();
        if (it->second.pinIndex < inputs.size()) return &inputs[it->second.pinIndex];
    } else {
        const auto& outputs = node->GetOutputs();
        if (it->second.pinIndex < outputs.size()) return &outputs[it->second.pinIndex];
    }
    return nullptr;
}

bool NodeGraph::HasPath(uint32_t startNodeId, uint32_t targetNodeId) const noexcept {
    if (startNodeId == targetNodeId) return true;

    std::queue<uint32_t> q;
    std::unordered_set<uint32_t> visited;

    q.push(startNodeId);
    visited.insert(startNodeId);

    while (!q.empty()) {
        const uint32_t curr = q.front();
        q.pop();

        if (curr == targetNodeId) return true;

        auto node = GetNode(curr);
        if (!node) continue;

        for (const auto& outPin : node->GetOutputs()) {
            for (const auto& [otherId, otherNode] : m_nodes) {
                if (!otherNode) continue;
                for (const auto& inPin : otherNode->GetInputs()) {
                    if (inPin.connectedPinId == outPin.id) {
                        if (otherId == targetNodeId) return true;
                        if (visited.find(otherId) == visited.end()) {
                            visited.insert(otherId);
                            q.push(otherId);
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool NodeGraph::Connect(uint32_t outputPinId, uint32_t inputPinId) {
    GraphPin* outPin = FindPin(outputPinId);
    GraphPin* inPin = FindPin(inputPinId);

    if (!outPin || !inPin) return false;
    if (outPin->direction != PinDirection::Output || inPin->direction != PinDirection::Input) return false;
    if (outPin->type != inPin->type) return false;
    if (outPin->nodeId == inPin->nodeId) return false;

    // Cycle prevention: if input pin's node can reach output pin's node, connecting would form a cycle
    if (HasPath(inPin->nodeId, outPin->nodeId)) {
        return false;
    }

    inPin->connectedPinId = outputPinId;
    MarkNodeDirty(inPin->nodeId);
    return true;
}

bool NodeGraph::Disconnect(uint32_t inputPinId) {
    GraphPin* inPin = FindPin(inputPinId);
    if (!inPin || inPin->direction != PinDirection::Input) return false;

    if (inPin->connectedPinId != 0) {
        inPin->connectedPinId = 0;
        MarkNodeDirty(inPin->nodeId);
        return true;
    }
    return false;
}

bool NodeGraph::RemoveNode(uint32_t nodeId) {
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) return false;

    auto nodeToRemove = it->second;
    for (auto& pin : nodeToRemove->GetInputs()) {
        Disconnect(pin.id);
    }
    for (auto& [otherId, node] : m_nodes) {
        if (otherId == nodeId) continue;
        for (auto& pin : node->GetInputs()) {
            for (const auto& outPin : nodeToRemove->GetOutputs()) {
                if (pin.connectedPinId == outPin.id) {
                    Disconnect(pin.id);
                }
            }
        }
    }

    for (const auto& pin : nodeToRemove->GetInputs()) {
        m_pinMap.erase(pin.id);
    }
    for (const auto& pin : nodeToRemove->GetOutputs()) {
        m_pinMap.erase(pin.id);
    }

    m_nodes.erase(it);
    return true;
}

std::shared_ptr<GraphNode> NodeGraph::GetNode(uint32_t id) const noexcept {
    auto it = m_nodes.find(id);
    if (it != m_nodes.end()) return it->second;
    return nullptr;
}

void NodeGraph::MarkNodeDirty(uint32_t nodeId) {
    auto node = GetNode(nodeId);
    if (!node) return;

    if (!node->IsDirty()) {
        node->MarkDirty();
    }

    for (const auto& outPin : node->GetOutputs()) {
        for (auto& [otherId, otherNode] : m_nodes) {
            if (otherId == nodeId || !otherNode) continue;
            for (const auto& inPin : otherNode->GetInputs()) {
                if (inPin.connectedPinId == outPin.id) {
                    if (!otherNode->IsDirty()) {
                        otherNode->MarkDirty();
                    }
                }
            }
        }
    }
}

std::vector<std::shared_ptr<GraphNode>> NodeGraph::TopologicalSort() const {
    std::vector<std::shared_ptr<GraphNode>> result;
    std::unordered_map<uint32_t, int> inDegree;
    std::unordered_map<uint32_t, std::vector<uint32_t>> adj;

    for (const auto& [id, node] : m_nodes) {
        inDegree[id] = 0;
    }

    for (const auto& [id, node] : m_nodes) {
        for (const auto& inPin : node->GetInputs()) {
            if (inPin.connectedPinId != 0) {
                const GraphPin* outPin = FindPin(inPin.connectedPinId);
                if (outPin) {
                    adj[outPin->nodeId].push_back(id);
                    inDegree[id]++;
                }
            }
        }
    }

    std::queue<uint32_t> q;
    for (const auto& [id, deg] : inDegree) {
        if (deg == 0) q.push(id);
    }

    while (!q.empty()) {
        const uint32_t u = q.front();
        q.pop();
        if (m_nodes.count(u)) {
            result.push_back(m_nodes.at(u));
        }

        auto it = adj.find(u);
        if (it != adj.end()) {
            for (const uint32_t v : it->second) {
                inDegree[v]--;
                if (inDegree[v] == 0) {
                    q.push(v);
                }
            }
        }
    }

    return result;
}

void NodeGraph::Evaluate() {
    auto sortedNodes = TopologicalSort();
    for (auto& node : sortedNodes) {
        for (auto& inPin : node->GetInputs()) {
            if (inPin.connectedPinId != 0) {
                const GraphPin* outPin = FindPin(inPin.connectedPinId);
                if (outPin) {
                    inPin.value = outPin->value;
                }
            }
        }

        if (node->IsDirty()) {
            node->Evaluate();
            node->ClearDirty();
        }
    }
}

} // namespace khepri::graph


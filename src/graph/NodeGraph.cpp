#include "NodeGraph.h"
#include <algorithm>
#include <queue>
#include <utility>

namespace khepri::graph {

uint32_t GraphNode::s_nextPinId = 1;

GraphNode::GraphNode(uint32_t id, std::string name, NodeDomain domain) noexcept
    : m_id(id), m_name(std::move(name)), m_domain(domain) {}

GraphPin& GraphNode::AddInput(const std::string& name, PinType type, PinValue defaultValue) {
    GraphPin pin;
    pin.id = s_nextPinId++;
    pin.nodeId = m_id;
    pin.name = name;
    pin.type = type;
    pin.direction = PinDirection::Input;
    pin.value = std::move(defaultValue);
    m_inputs.push_back(pin);
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

bool NodeGraph::Connect(uint32_t outputPinId, uint32_t inputPinId) {
    GraphPin* outPin = nullptr;
    GraphPin* inPin = nullptr;

    for (auto& [id, node] : m_nodes) {
        for (auto& pin : node->GetOutputs()) {
            if (pin.id == outputPinId) outPin = &pin;
        }
        for (auto& pin : node->GetInputs()) {
            if (pin.id == inputPinId) inPin = &pin;
        }
    }

    if (!outPin || !inPin) return false;
    if (outPin->direction != PinDirection::Output || inPin->direction != PinDirection::Input) return false;
    if (outPin->type != inPin->type) return false;

    inPin->connectedPinId = outputPinId;
    m_pinMap[outPin->id] = outPin;
    m_pinMap[inPin->id] = inPin;

    MarkNodeDirty(inPin->nodeId);
    return true;
}

bool NodeGraph::Disconnect(uint32_t inputPinId) {
    for (auto& [id, node] : m_nodes) {
        for (auto& pin : node->GetInputs()) {
            if (pin.id == inputPinId) {
                pin.connectedPinId = 0;
                node->MarkDirty();
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<GraphNode> NodeGraph::GetNode(uint32_t id) const noexcept {
    auto it = m_nodes.find(id);
    if (it != m_nodes.end()) return it->second;
    return nullptr;
}

void NodeGraph::MarkNodeDirty(uint32_t nodeId) {
    auto node = GetNode(nodeId);
    if (!node || node->IsDirty()) return;

    node->MarkDirty();

    for (const auto& outPin : node->GetOutputs()) {
        for (auto& [otherId, otherNode] : m_nodes) {
            for (const auto& inPin : otherNode->GetInputs()) {
                if (inPin.connectedPinId == outPin.id) {
                    MarkNodeDirty(otherNode->GetId());
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
                for (const auto& [parentId, parentNode] : m_nodes) {
                    for (const auto& outPin : parentNode->GetOutputs()) {
                        if (outPin.id == inPin.connectedPinId) {
                            adj[parentId].push_back(id);
                            inDegree[id]++;
                        }
                    }
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

        for (const uint32_t v : adj[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) {
                q.push(v);
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
                auto it = m_pinMap.find(inPin.connectedPinId);
                if (it != m_pinMap.end() && it->second) {
                    inPin.value = it->second->value;
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

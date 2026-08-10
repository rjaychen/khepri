#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <glm/glm.hpp>

#include "../scene/MeshComponent.h"

namespace khepri::graph {

enum class PinType {
    GeometryBuffer,
    Material,
    Float,
    Vector3,
    Matrix4
};

enum class PinDirection {
    Input,
    Output
};

enum class NodeDomain {
    Geometry,
    Material,
    Animation
};

using PinValue = std::variant<float, glm::vec3, glm::mat4, std::string, std::shared_ptr<MeshComponent>>;

struct GraphPin {
    uint32_t id{0};
    uint32_t nodeId{0};
    std::string name;
    PinType type{PinType::Float};
    PinDirection direction{PinDirection::Input};
    PinValue value{0.0f};
    uint32_t connectedPinId{0};
};

class GraphNode {
public:
    inline static bool s_enableGraphLogging{false};

    GraphNode(uint32_t id, std::string name, NodeDomain domain) noexcept;
    virtual ~GraphNode() noexcept = default;

    GraphNode(const GraphNode&) = delete;
    GraphNode& operator=(const GraphNode&) = delete;
    GraphNode(GraphNode&&) noexcept = default;
    GraphNode& operator=(GraphNode&&) noexcept = default;

    [[nodiscard]] uint32_t GetId() const noexcept { return m_id; }
    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    [[nodiscard]] NodeDomain GetDomain() const noexcept { return m_domain; }

    [[nodiscard]] bool IsDirty() const noexcept { return m_isDirty; }
    void MarkDirty() noexcept { m_isDirty = true; }
    void ClearDirty() noexcept { m_isDirty = false; }

    [[nodiscard]] std::vector<GraphPin>& GetInputs() noexcept { return m_inputs; }
    [[nodiscard]] const std::vector<GraphPin>& GetInputs() const noexcept { return m_inputs; }
    [[nodiscard]] std::vector<GraphPin>& GetOutputs() noexcept { return m_outputs; }
    [[nodiscard]] const std::vector<GraphPin>& GetOutputs() const noexcept { return m_outputs; }

    [[nodiscard]] GraphPin* FindInput(const std::string& name) noexcept;
    [[nodiscard]] const GraphPin* FindInput(const std::string& name) const noexcept;
    [[nodiscard]] GraphPin* FindOutput(const std::string& name) noexcept;
    [[nodiscard]] const GraphPin* FindOutput(const std::string& name) const noexcept;

    virtual void Evaluate() = 0;

protected:
    GraphPin& AddInput(const std::string& name, PinType type, PinValue defaultValue = 0.0f);
    GraphPin& AddOutput(const std::string& name, PinType type);

    uint32_t m_id;
    std::string m_name;
    NodeDomain m_domain;
    bool m_isDirty{true};
    std::vector<GraphPin> m_inputs;
    std::vector<GraphPin> m_outputs;
    static uint32_t s_nextPinId;
};

class NodeGraph {
public:
    NodeGraph() noexcept = default;
    ~NodeGraph() noexcept = default;

    NodeGraph(const NodeGraph&) = delete;
    NodeGraph& operator=(const NodeGraph&) = delete;
    NodeGraph(NodeGraph&&) noexcept = default;
    NodeGraph& operator=(NodeGraph&&) noexcept = default;

    template<typename T, typename... Args>
    std::shared_ptr<T> CreateNode(Args&&... args) {
        const uint32_t id = ++m_nextNodeId;
        auto node = std::make_shared<T>(id, std::forward<Args>(args)...);
        m_nodes[id] = node;
        return node;
    }

    [[nodiscard]] bool Connect(uint32_t outputPinId, uint32_t inputPinId);
    bool Disconnect(uint32_t inputPinId);

    void Evaluate();
    void MarkNodeDirty(uint32_t nodeId);

    [[nodiscard]] const std::unordered_map<uint32_t, std::shared_ptr<GraphNode>>& GetNodes() const noexcept { return m_nodes; }
    [[nodiscard]] std::shared_ptr<GraphNode> GetNode(uint32_t id) const noexcept;

    [[nodiscard]] std::vector<std::shared_ptr<GraphNode>> TopologicalSort() const;

private:
    uint32_t m_nextNodeId{0};
    std::unordered_map<uint32_t, std::shared_ptr<GraphNode>> m_nodes;
    std::unordered_map<uint32_t, GraphPin*> m_pinMap;
};

} // namespace khepri::graph

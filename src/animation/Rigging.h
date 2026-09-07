#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../core/StrongId.h"

namespace khepri {

struct JointTag {};
using JointId = StrongId<JointTag, uint32_t>;
inline constexpr JointId InvalidJointId = JointId::Invalid();

} // namespace khepri

struct Joint {
    std::string name;
    khepri::JointId parentId = khepri::InvalidJointId;
    int32_t parentIndex = -1;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 inverseBindMatrix{1.0f};

    [[nodiscard]] glm::mat4 GetLocalMatrix() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::toMat4(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }
};

class Skeleton {
public:
    Skeleton() = default;

    khepri::JointId AddJoint(const std::string& name, khepri::JointId parentId = khepri::InvalidJointId, const glm::mat4& bindMatrix = glm::mat4(1.0f));
    int32_t AddJoint(const std::string& name, int32_t parentIndex, const glm::mat4& bindMatrix = glm::mat4(1.0f));

    void ComputeSkinningMatrices(std::vector<glm::mat4>& outMatrices) const;

    [[nodiscard]] Joint* GetJoint(khepri::JointId id) {
        return (id.IsValid() && id.Get() < m_joints.size()) ? &m_joints[id.Get()] : nullptr;
    }
    [[nodiscard]] const Joint* GetJoint(khepri::JointId id) const {
        return (id.IsValid() && id.Get() < m_joints.size()) ? &m_joints[id.Get()] : nullptr;
    }

    [[nodiscard]] Joint* GetJoint(int32_t index) {
        return (index >= 0 && static_cast<size_t>(index) < m_joints.size()) ? &m_joints[index] : nullptr;
    }
    [[nodiscard]] const Joint* GetJoint(int32_t index) const {
        return (index >= 0 && static_cast<size_t>(index) < m_joints.size()) ? &m_joints[index] : nullptr;
    }

    [[nodiscard]] khepri::JointId FindJointId(const std::string& name) const;
    [[nodiscard]] int32_t FindJointIndex(const std::string& name) const;

    [[nodiscard]] const std::vector<Joint>& GetJoints() const noexcept { return m_joints; }

private:
    std::vector<Joint> m_joints;
    std::unordered_map<std::string, uint32_t> m_jointNameToIndex;
};


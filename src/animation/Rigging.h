#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct Joint {
    std::string name;
    int32_t parentIndex = -1;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 inverseBindMatrix{1.0f};

    glm::mat4 GetLocalMatrix() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::toMat4(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }
};

class Skeleton {
public:
    Skeleton() = default;

    int32_t AddJoint(const std::string& name, int32_t parentIndex = -1, const glm::mat4& bindMatrix = glm::mat4(1.0f));

    void ComputeSkinningMatrices(std::vector<glm::mat4>& outMatrices) const;

    Joint* GetJoint(int32_t index) { return (index >= 0 && index < (int32_t)m_joints.size()) ? &m_joints[index] : nullptr; }
    int32_t FindJointIndex(const std::string& name) const;

    const std::vector<Joint>& GetJoints() const { return m_joints; }

private:
    std::vector<Joint> m_joints;
    std::unordered_map<std::string, int32_t> m_jointNameToIndex;
};

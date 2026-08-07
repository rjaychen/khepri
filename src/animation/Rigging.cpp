#include "Rigging.h"

int32_t Skeleton::AddJoint(const std::string& name, int32_t parentIndex, const glm::mat4& bindMatrix) {
    Joint joint;
    joint.name = name;
    joint.parentIndex = parentIndex;
    joint.inverseBindMatrix = glm::inverse(bindMatrix);

    int32_t idx = static_cast<int32_t>(m_joints.size());
    m_joints.push_back(joint);
    m_jointNameToIndex[name] = idx;
    return idx;
}

void Skeleton::ComputeSkinningMatrices(std::vector<glm::mat4>& outMatrices) const {
    outMatrices.resize(m_joints.size());
    std::vector<glm::mat4> globalTransforms(m_joints.size());

    for (size_t i = 0; i < m_joints.size(); ++i) {
        glm::mat4 local = m_joints[i].GetLocalMatrix();
        if (m_joints[i].parentIndex >= 0) {
            globalTransforms[i] = globalTransforms[m_joints[i].parentIndex] * local;
        } else {
            globalTransforms[i] = local;
        }
        outMatrices[i] = globalTransforms[i] * m_joints[i].inverseBindMatrix;
    }
}

int32_t Skeleton::FindJointIndex(const std::string& name) const {
    auto it = m_jointNameToIndex.find(name);
    return (it != m_jointNameToIndex.end()) ? it->second : -1;
}

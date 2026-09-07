#include "Rigging.h"

khepri::JointId Skeleton::AddJoint(const std::string& name, khepri::JointId parentId, const glm::mat4& bindMatrix) {
    Joint joint;
    joint.name = name;
    joint.parentId = parentId;
    joint.parentIndex = parentId.IsValid() ? static_cast<int32_t>(parentId.Get()) : -1;
    joint.inverseBindMatrix = glm::inverse(bindMatrix);

    uint32_t idx = static_cast<uint32_t>(m_joints.size());
    m_joints.push_back(joint);
    m_jointNameToIndex[name] = idx;
    return khepri::JointId(idx);
}

int32_t Skeleton::AddJoint(const std::string& name, int32_t parentIndex, const glm::mat4& bindMatrix) {
    khepri::JointId parentId = (parentIndex >= 0) ? khepri::JointId(static_cast<uint32_t>(parentIndex)) : khepri::InvalidJointId;
    return static_cast<int32_t>(AddJoint(name, parentId, bindMatrix).Get());
}

void Skeleton::ComputeSkinningMatrices(std::vector<glm::mat4>& outMatrices) const {
    outMatrices.resize(m_joints.size());
    std::vector<glm::mat4> globalTransforms(m_joints.size());

    for (size_t i = 0; i < m_joints.size(); ++i) {
        glm::mat4 local = m_joints[i].GetLocalMatrix();
        if (m_joints[i].parentId.IsValid() && m_joints[i].parentId.Get() < m_joints.size()) {
            globalTransforms[i] = globalTransforms[m_joints[i].parentId.Get()] * local;
        } else if (m_joints[i].parentIndex >= 0 && static_cast<size_t>(m_joints[i].parentIndex) < m_joints.size()) {
            globalTransforms[i] = globalTransforms[static_cast<size_t>(m_joints[i].parentIndex)] * local;
        } else {
            globalTransforms[i] = local;
        }
        outMatrices[i] = globalTransforms[i] * m_joints[i].inverseBindMatrix;
    }
}

khepri::JointId Skeleton::FindJointId(const std::string& name) const {
    auto it = m_jointNameToIndex.find(name);
    return (it != m_jointNameToIndex.end()) ? khepri::JointId(it->second) : khepri::InvalidJointId;
}

int32_t Skeleton::FindJointIndex(const std::string& name) const {
    auto id = FindJointId(name);
    return id.IsValid() ? static_cast<int32_t>(id.Get()) : -1;
}


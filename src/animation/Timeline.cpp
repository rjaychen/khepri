#include "Timeline.h"
#include <algorithm>

glm::vec3 AnimationTrack::SamplePosition(float time) const {
    if (positionKeys.empty()) return glm::vec3(0.0f);
    if (positionKeys.size() == 1 || time <= positionKeys.front().time) return positionKeys.front().value;
    if (time >= positionKeys.back().time) return positionKeys.back().value;

    for (size_t i = 0; i < positionKeys.size() - 1; ++i) {
        if (time >= positionKeys[i].time && time <= positionKeys[i + 1].time) {
            float t = (time - positionKeys[i].time) / (positionKeys[i + 1].time - positionKeys[i].time);
            return glm::mix(positionKeys[i].value, positionKeys[i + 1].value, t);
        }
    }
    return positionKeys.front().value;
}

glm::quat AnimationTrack::SampleRotation(float time) const {
    if (rotationKeys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (rotationKeys.size() == 1 || time <= rotationKeys.front().time) return rotationKeys.front().value;
    if (time >= rotationKeys.back().time) return rotationKeys.back().value;

    for (size_t i = 0; i < rotationKeys.size() - 1; ++i) {
        if (time >= rotationKeys[i].time && time <= rotationKeys[i + 1].time) {
            float t = (time - rotationKeys[i].time) / (rotationKeys[i + 1].time - rotationKeys[i].time);
            return glm::slerp(rotationKeys[i].value, rotationKeys[i + 1].value, t);
        }
    }
    return rotationKeys.front().value;
}

glm::vec3 AnimationTrack::SampleScale(float time) const {
    if (scaleKeys.empty()) return glm::vec3(1.0f);
    if (scaleKeys.size() == 1 || time <= scaleKeys.front().time) return scaleKeys.front().value;
    if (time >= scaleKeys.back().time) return scaleKeys.back().value;

    for (size_t i = 0; i < scaleKeys.size() - 1; ++i) {
        if (time >= scaleKeys[i].time && time <= scaleKeys[i + 1].time) {
            float t = (time - scaleKeys[i].time) / (scaleKeys[i + 1].time - scaleKeys[i].time);
            return glm::mix(scaleKeys[i].value, scaleKeys[i + 1].value, t);
        }
    }
    return scaleKeys.front().value;
}

AnimationTrack* AnimationClip::GetOrCreateTrack(const std::string& targetNodeName) {
    for (auto& track : tracks) {
        if (track.targetNodeName == targetNodeName) return &track;
    }
    tracks.push_back({targetNodeName, {}, {}, {}});
    return &tracks.back();
}

void AnimationClip::AddOrUpdatePositionKey(const std::string& targetNodeName, float time, const glm::vec3& position) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    for (auto& k : track->positionKeys) {
        if (std::abs(k.time - time) < 1e-4f) {
            k.value = position;
            return;
        }
    }
    track->positionKeys.push_back({time, position});
    std::sort(track->positionKeys.begin(), track->positionKeys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

void AnimationClip::AddOrUpdateRotationKey(const std::string& targetNodeName, float time, const glm::quat& rotation) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    for (auto& k : track->rotationKeys) {
        if (std::abs(k.time - time) < 1e-4f) {
            k.value = rotation;
            return;
        }
    }
    track->rotationKeys.push_back({time, rotation});
    std::sort(track->rotationKeys.begin(), track->rotationKeys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

void AnimationClip::AddOrUpdateScaleKey(const std::string& targetNodeName, float time, const glm::vec3& scale) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    for (auto& k : track->scaleKeys) {
        if (std::abs(k.time - time) < 1e-4f) {
            k.value = scale;
            return;
        }
    }
    track->scaleKeys.push_back({time, scale});
    std::sort(track->scaleKeys.begin(), track->scaleKeys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

Timeline::Timeline() = default;

void Timeline::SetClip(std::shared_ptr<AnimationClip> clip) {
    m_clip = clip;
    m_currentTime = 0.0f;
}

void Timeline::SetCurrentTime(float time, SceneNode* rootSceneNode) {
    m_currentTime = time;
    if (m_clip) {
        if (m_currentTime > m_clip->duration) {
            m_currentTime = m_isLooping ? fmod(m_currentTime, m_clip->duration) : m_clip->duration;
        } else if (m_currentTime < 0.0f) {
            m_currentTime = 0.0f;
        }
    }
    if (rootSceneNode) {
        EvaluateAtTime(m_currentTime, rootSceneNode);
    }
}

void Timeline::KeyframeNodePose(const SceneNode* node) {
    if (!node || !m_clip) return;
    glm::quat rotQuat = glm::quat(glm::radians(node->rotationDegrees));
    m_clip->AddOrUpdatePositionKey(node->name, m_currentTime, node->position);
    m_clip->AddOrUpdateRotationKey(node->name, m_currentTime, rotQuat);
    m_clip->AddOrUpdateScaleKey(node->name, m_currentTime, node->scale);
}

static SceneNode* FindNodeByName(SceneNode* node, const std::string& name) {
    if (!node) return nullptr;
    if (node->name == name) return node;
    for (const auto& child : node->GetChildren()) {
        SceneNode* found = FindNodeByName(child.get(), name);
        if (found) return found;
    }
    return nullptr;
}

void Timeline::EvaluateAtTime(float time, SceneNode* rootSceneNode, Skeleton* skeleton) {
    if (!m_clip || !rootSceneNode) return;

    for (const auto& track : m_clip->tracks) {
        SceneNode* targetNode = FindNodeByName(rootSceneNode, track.targetNodeName);
        if (targetNode) {
            if (!track.positionKeys.empty())
                targetNode->position = track.SamplePosition(time);
            if (!track.rotationKeys.empty()) {
                glm::quat q = track.SampleRotation(time);
                targetNode->rotationDegrees = glm::degrees(glm::eulerAngles(q));
            }
            if (!track.scaleKeys.empty())
                targetNode->scale = track.SampleScale(time);
            targetNode->SyncPropertiesToTransform();
        }

        if (skeleton) {
            int32_t jIdx = skeleton->FindJointIndex(track.targetNodeName);
            if (jIdx >= 0) {
                Joint* joint = skeleton->GetJoint(jIdx);
                if (joint) {
                    if (!track.positionKeys.empty()) joint->position = track.SamplePosition(time);
                    if (!track.rotationKeys.empty()) joint->rotation = track.SampleRotation(time);
                    if (!track.scaleKeys.empty())    joint->scale    = track.SampleScale(time);
                }
            }
        }
    }
}

void Timeline::Update(float deltaTime, SceneNode* rootSceneNode, Skeleton* skeleton) {
    if (!m_clip) return;

    if (m_isPlaying) {
        m_currentTime += deltaTime * m_playbackSpeed;
        if (m_currentTime >= m_clip->duration) {
            if (m_isLooping) {
                m_currentTime = fmod(m_currentTime, m_clip->duration);
            } else {
                m_currentTime = m_clip->duration;
                m_isPlaying = false;
            }
        }

        EvaluateAtTime(m_currentTime, rootSceneNode, skeleton);
    }
}

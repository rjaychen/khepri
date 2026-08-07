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

Timeline::Timeline() = default;

void Timeline::SetClip(std::shared_ptr<AnimationClip> clip) {
    m_clip = clip;
    m_currentTime = 0.0f;
}

void Timeline::SetCurrentTime(float time) {
    m_currentTime = time;
    if (m_clip) {
        if (m_currentTime > m_clip->duration) {
            m_currentTime = m_isLooping ? fmod(m_currentTime, m_clip->duration) : m_clip->duration;
        } else if (m_currentTime < 0.0f) {
            m_currentTime = 0.0f;
        }
    }
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
    }

    for (const auto& track : m_clip->tracks) {
        SceneNode* targetNode = FindNodeByName(rootSceneNode, track.targetNodeName);
        if (targetNode) {
            if (!track.positionKeys.empty()) {
                targetNode->position = track.SamplePosition(m_currentTime);
            }
            if (!track.rotationKeys.empty()) {
                glm::quat q = track.SampleRotation(m_currentTime);
                targetNode->rotationDegrees = glm::degrees(glm::eulerAngles(q));
            }
            if (!track.scaleKeys.empty()) {
                targetNode->scale = track.SampleScale(m_currentTime);
            }
            targetNode->SyncPropertiesToTransform();
        }

        if (skeleton) {
            int32_t jIdx = skeleton->FindJointIndex(track.targetNodeName);
            if (jIdx >= 0) {
                Joint* joint = skeleton->GetJoint(jIdx);
                if (joint) {
                    if (!track.positionKeys.empty()) joint->position = track.SamplePosition(m_currentTime);
                    if (!track.rotationKeys.empty()) joint->rotation = track.SampleRotation(m_currentTime);
                    if (!track.scaleKeys.empty()) joint->scale = track.SampleScale(m_currentTime);
                }
            }
        }
    }
}

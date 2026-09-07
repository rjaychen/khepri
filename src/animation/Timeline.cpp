#include "Timeline.h"
#include <algorithm>
#include <unordered_map>

namespace {

// Locates the key segment [i, i+1] containing `time` in a sorted key vector via
// binary search (O(log n) instead of the previous O(n) linear scan). Returns
// the index i of the left key of the containing segment. Caller guarantees
// keys.size() >= 2 and keys.front().time <= time < keys.back().time.
template <typename Keyvec>
size_t FindSegment(const Keyvec& keys, float time) {
    // upper_bound: first key with time strictly greater than `time`.
    auto it = std::upper_bound(keys.begin(), keys.end(), time,
                               [](float t, const auto& k) { return t < k.time; });
    // It is never begin() (time >= front) and never end() (time < back), so the
    // left key of the segment is the element before it.
    size_t hi = static_cast<size_t>(std::distance(keys.begin(), it));
    return hi - 1;
}

// Normalized [0,1] parameter within a segment, honoring InterpolationMode.
// Step holds the left key's value (returns 0); Linear/CubicSpline currently
// share linear parameterization (true cubic-spline tangents are future work,
// but the enum is no longer silently ignored - Step now behaves distinctly).
float SegmentParam(float time, float t0, float t1, InterpolationMode mode) {
    if (mode == InterpolationMode::Step) return 0.0f;
    const float span = t1 - t0;
    return span > 0.0f ? ((time - t0) / span) : 0.0f;
}

template<typename T>
T SampleTrackInternal(const std::vector<Keyframe<T>>& keys, float time, const T& defaultValue) {
    if (keys.empty()) return defaultValue;
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    const size_t i = FindSegment(keys, time);
    const float t = SegmentParam(time, keys[i].time, keys[i + 1].time, keys[i].interpolation);

    if constexpr (std::is_same_v<T, glm::quat>) {
        return glm::slerp(keys[i].value, keys[i + 1].value, t);
    } else {
        return glm::mix(keys[i].value, keys[i + 1].value, t);
    }
}

template<typename T>
void AddOrUpdateKeyInternal(std::vector<Keyframe<T>>& keys, float time, const T& value, InterpolationMode mode) {
    for (auto& k : keys) {
        if (std::abs(k.time - time) < 1e-4f) {
            k.value = value;
            k.interpolation = mode;
            return;
        }
    }
    keys.push_back(Keyframe<T>{.time = time, .value = value, .interpolation = mode});
    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

// Build a name->node lookup for the whole subtree in one traversal, so
// EvaluateAtTime resolves each track's target in O(1) instead of running a
// full-tree DFS per track per frame.
static void BuildNodeIndex(SceneNode* node, std::unordered_map<std::string, SceneNode*>& index) {
    if (!node) return;
    // First writer wins (matches FindNodeByName's pre-order first-match semantics).
    index.emplace(node->name, node);
    for (const auto& child : node->GetChildren()) {
        BuildNodeIndex(child.get(), index);
    }
}

} // namespace

template<typename T>
T AnimationTrack::Sample(float time) const {
    if constexpr (std::is_same_v<T, glm::vec3>) {
        return SamplePosition(time);
    } else if constexpr (std::is_same_v<T, glm::quat>) {
        return SampleRotation(time);
    } else {
        return T{};
    }
}

template glm::vec3 AnimationTrack::Sample<glm::vec3>(float time) const;
template glm::quat AnimationTrack::Sample<glm::quat>(float time) const;

glm::vec3 AnimationTrack::SamplePosition(float time) const {
    return SampleTrackInternal(positionKeys, time, glm::vec3(0.0f));
}

glm::quat AnimationTrack::SampleRotation(float time) const {
    return SampleTrackInternal(rotationKeys, time, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
}

glm::vec3 AnimationTrack::SampleScale(float time) const {
    return SampleTrackInternal(scaleKeys, time, glm::vec3(1.0f));
}

AnimationTrack* AnimationClip::GetOrCreateTrack(const std::string& targetNodeName) {
    for (auto& track : tracks) {
        if (track.targetNodeName == targetNodeName) return &track;
    }
    tracks.push_back({targetNodeName, {}, {}, {}});
    return &tracks.back();
}

template<typename T>
void AnimationClip::AddOrUpdateKey(const std::string& targetNodeName, float time, const T& value, InterpolationMode mode) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    if constexpr (std::is_same_v<T, glm::vec3>) {
        AddOrUpdateKeyInternal(track->positionKeys, time, value, mode);
    } else if constexpr (std::is_same_v<T, glm::quat>) {
        AddOrUpdateKeyInternal(track->rotationKeys, time, value, mode);
    }
}

template void AnimationClip::AddOrUpdateKey<glm::vec3>(const std::string&, float, const glm::vec3&, InterpolationMode);
template void AnimationClip::AddOrUpdateKey<glm::quat>(const std::string&, float, const glm::quat&, InterpolationMode);

void AnimationClip::AddOrUpdatePositionKey(const std::string& targetNodeName, float time, const glm::vec3& position, InterpolationMode mode) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    AddOrUpdateKeyInternal(track->positionKeys, time, position, mode);
}

void AnimationClip::AddOrUpdateRotationKey(const std::string& targetNodeName, float time, const glm::quat& rotation, InterpolationMode mode) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    AddOrUpdateKeyInternal(track->rotationKeys, time, rotation, mode);
}

void AnimationClip::AddOrUpdateScaleKey(const std::string& targetNodeName, float time, const glm::vec3& scale, InterpolationMode mode) {
    AnimationTrack* track = GetOrCreateTrack(targetNodeName);
    if (!track) return;
    AddOrUpdateKeyInternal(track->scaleKeys, time, scale, mode);
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

void Timeline::EvaluateAtTime(float time, SceneNode* rootSceneNode, Skeleton* skeleton) {
    if (!m_clip || !rootSceneNode) return;

    std::unordered_map<std::string, SceneNode*> nodeIndex;
    BuildNodeIndex(rootSceneNode, nodeIndex);

    for (const auto& track : m_clip->tracks) {
        auto it = nodeIndex.find(track.targetNodeName);
        SceneNode* targetNode = (it != nodeIndex.end()) ? it->second : nullptr;
        if (targetNode) {
            if (!track.positionKeys.empty())
                targetNode->position = track.SamplePosition(time);
            if (!track.rotationKeys.empty()) {
                glm::quat q = glm::normalize(track.SampleRotation(time));
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

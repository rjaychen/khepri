#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../scene/SceneNode.h"
#include "Rigging.h"

enum class InterpolationMode {
    Step,
    Linear,
    CubicSpline
};

template<typename T>
struct Keyframe {
    float time;
    T value;
    InterpolationMode interpolation = InterpolationMode::Linear;
};

struct AnimationTrack {
    std::string targetNodeName;
    std::vector<Keyframe<glm::vec3>> positionKeys;
    std::vector<Keyframe<glm::quat>> rotationKeys;
    std::vector<Keyframe<glm::vec3>> scaleKeys;

    glm::vec3 SamplePosition(float time) const;
    glm::quat SampleRotation(float time) const;
    glm::vec3 SampleScale(float time) const;
};

struct AnimationClip {
    std::string name;
    float duration = 5.0f;
    float ticksPerSecond = 24.0f;
    std::vector<AnimationTrack> tracks;
};

class Timeline {
public:
    Timeline();

    void SetClip(std::shared_ptr<AnimationClip> clip);
    void Play() { m_isPlaying = true; }
    void Pause() { m_isPlaying = false; }
    void Stop() { m_isPlaying = false; m_currentTime = 0.0f; }

    void SetCurrentTime(float time);
    float GetCurrentTime() const { return m_currentTime; }
    float GetDuration() const { return m_clip ? m_clip->duration : 0.0f; }
    bool IsPlaying() const { return m_isPlaying; }
    bool IsLooping() const { return m_isLooping; }
    void SetLooping(bool loop) { m_isLooping = loop; }

    void Update(float deltaTime, SceneNode* rootSceneNode, Skeleton* skeleton = nullptr);

    std::shared_ptr<AnimationClip> GetCurrentClip() const { return m_clip; }

private:
    std::shared_ptr<AnimationClip> m_clip;
    float m_currentTime = 0.0f;
    float m_playbackSpeed = 1.0f;
    bool m_isPlaying = false;
    bool m_isLooping = true;
};

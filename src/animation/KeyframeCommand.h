#pragma once

#include "../core/Command.h"
#include "Timeline.h"
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace khepri::animation {

/**
 * @brief Command to add or update a position keyframe on an animation clip track.
 */
class AddPositionKeyframeCommand : public core::ICommand {
public:
    AddPositionKeyframeCommand(
        std::shared_ptr<AnimationClip> clip,
        std::string targetNodeName,
        float time,
        glm::vec3 position,
        InterpolationMode mode = InterpolationMode::Linear,
        std::string name = "Add Position Keyframe"
    ) : m_clip(std::move(clip)), m_targetNodeName(std::move(targetNodeName)),
        m_time(time), m_value(position), m_mode(mode), m_name(std::move(name)) {
        CapturePreviousKey();
    }

    void Execute() override {
        if (!m_clip) return;
        CapturePreviousKey();
        m_clip->AddOrUpdatePositionKey(m_targetNodeName, m_time, m_value, m_mode);
    }

    void Undo() override {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        auto it = std::find_if(track->positionKeys.begin(), track->positionKeys.end(),
            [this](const auto& k) { return std::abs(k.time - m_time) < 1e-4f; });

        if (m_hadPreviousKey) {
            if (it != track->positionKeys.end()) {
                *it = m_previousKey;
            } else {
                track->positionKeys.push_back(m_previousKey);
                std::sort(track->positionKeys.begin(), track->positionKeys.end(),
                    [](const auto& a, const auto& b) { return a.time < b.time; });
            }
        } else {
            if (it != track->positionKeys.end()) {
                track->positionKeys.erase(it);
            }
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    void CapturePreviousKey() {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        for (const auto& k : track->positionKeys) {
            if (std::abs(k.time - m_time) < 1e-4f) {
                m_hadPreviousKey = true;
                m_previousKey = k;
                return;
            }
        }
        m_hadPreviousKey = false;
    }

    std::shared_ptr<AnimationClip> m_clip;
    std::string m_targetNodeName;
    float m_time{0.0f};
    glm::vec3 m_value{0.0f};
    InterpolationMode m_mode{InterpolationMode::Linear};
    bool m_hadPreviousKey{false};
    Keyframe<glm::vec3> m_previousKey{};
    std::string m_name;
};

/**
 * @brief Command to add or update a rotation keyframe on an animation clip track.
 */
class AddRotationKeyframeCommand : public core::ICommand {
public:
    AddRotationKeyframeCommand(
        std::shared_ptr<AnimationClip> clip,
        std::string targetNodeName,
        float time,
        glm::quat rotation,
        InterpolationMode mode = InterpolationMode::Linear,
        std::string name = "Add Rotation Keyframe"
    ) : m_clip(std::move(clip)), m_targetNodeName(std::move(targetNodeName)),
        m_time(time), m_value(rotation), m_mode(mode), m_name(std::move(name)) {
        CapturePreviousKey();
    }

    void Execute() override {
        if (!m_clip) return;
        CapturePreviousKey();
        m_clip->AddOrUpdateRotationKey(m_targetNodeName, m_time, m_value, m_mode);
    }

    void Undo() override {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        auto it = std::find_if(track->rotationKeys.begin(), track->rotationKeys.end(),
            [this](const auto& k) { return std::abs(k.time - m_time) < 1e-4f; });

        if (m_hadPreviousKey) {
            if (it != track->rotationKeys.end()) {
                *it = m_previousKey;
            } else {
                track->rotationKeys.push_back(m_previousKey);
                std::sort(track->rotationKeys.begin(), track->rotationKeys.end(),
                    [](const auto& a, const auto& b) { return a.time < b.time; });
            }
        } else {
            if (it != track->rotationKeys.end()) {
                track->rotationKeys.erase(it);
            }
        }
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    void CapturePreviousKey() {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        for (const auto& k : track->rotationKeys) {
            if (std::abs(k.time - m_time) < 1e-4f) {
                m_hadPreviousKey = true;
                m_previousKey = k;
                return;
            }
        }
        m_hadPreviousKey = false;
    }

    std::shared_ptr<AnimationClip> m_clip;
    std::string m_targetNodeName;
    float m_time{0.0f};
    glm::quat m_value{1.0f, 0.0f, 0.0f, 0.0f};
    InterpolationMode m_mode{InterpolationMode::Linear};
    bool m_hadPreviousKey{false};
    Keyframe<glm::quat> m_previousKey{};
    std::string m_name;
};

/**
 * @brief Command to remove a keyframe from an animation track.
 */
template<typename T>
class RemoveKeyframeCommand : public core::ICommand {
public:
    RemoveKeyframeCommand(
        std::shared_ptr<AnimationClip> clip,
        std::string targetNodeName,
        float time,
        std::string name = "Remove Keyframe"
    ) : m_clip(std::move(clip)), m_targetNodeName(std::move(targetNodeName)),
        m_time(time), m_name(std::move(name)) {
        CaptureKey();
    }

    void Execute() override {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        auto& keys = GetKeys(track);
        auto it = std::find_if(keys.begin(), keys.end(),
            [this](const auto& k) { return std::abs(k.time - m_time) < 1e-4f; });
        if (it != keys.end()) {
            m_capturedKey = *it;
            m_hasCapturedKey = true;
            keys.erase(it);
        }
    }

    void Undo() override {
        if (!m_clip || !m_hasCapturedKey) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        auto& keys = GetKeys(track);
        keys.push_back(m_capturedKey);
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return m_name;
    }

private:
    static std::vector<Keyframe<T>>& GetKeys(AnimationTrack* track) {
        if constexpr (std::is_same_v<T, glm::vec3>) {
            return track->positionKeys;
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            return track->rotationKeys;
        }
    }

    void CaptureKey() {
        if (!m_clip) return;
        auto track = m_clip->GetOrCreateTrack(m_targetNodeName);
        if (!track) return;

        const auto& keys = GetKeys(track);
        auto it = std::find_if(keys.begin(), keys.end(),
            [this](const auto& k) { return std::abs(k.time - m_time) < 1e-4f; });
        if (it != keys.end()) {
            m_capturedKey = *it;
            m_hasCapturedKey = true;
        }
    }

    std::shared_ptr<AnimationClip> m_clip;
    std::string m_targetNodeName;
    float m_time{0.0f};
    bool m_hasCapturedKey{false};
    Keyframe<T> m_capturedKey{};
    std::string m_name;
};

} // namespace khepri::animation

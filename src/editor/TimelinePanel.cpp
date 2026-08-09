#include "TimelinePanel.h"

void TimelinePanel::RenderUI(Timeline& timeline, SceneNode* selectedNode, SceneNode* rootSceneNode) {
    ImGui::Begin("Animation Timeline");

    bool isPlaying = timeline.IsPlaying();
    if (isPlaying) {
        if (ImGui::Button("Pause")) timeline.Pause();
    } else {
        if (ImGui::Button("Play")) timeline.Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) timeline.Stop();

    ImGui::SameLine();
    bool looping = timeline.IsLooping();
    if (ImGui::Checkbox("Loop", &looping)) {
        timeline.SetLooping(looping);
    }

    float currentTime = timeline.GetCurrentTime();
    float duration = timeline.GetDuration();
    if (duration <= 0.0f) duration = 5.0f;

    ImGui::SameLine();
    ImGui::Text("Time: %.2f / %.2f s", currentTime, duration);

    if (ImGui::SliderFloat("Playhead", &currentTime, 0.0f, duration, "%.2f s")) {
        timeline.SetCurrentTime(currentTime, rootSceneNode);
    }

    ImGui::Separator();

    // Keyframing controls
    if (selectedNode) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Selected Target: %s", selectedNode->name.c_str());
        if (ImGui::Button("Keyframe Current Pose")) {
            timeline.KeyframeNodePose(selectedNode);
        }
    } else {
        ImGui::TextDisabled("Select a node in Scene Hierarchy to keyframe pose");
    }

    ImGui::SameLine();
    if (ImGui::Button("New Clip")) {
        auto newClip = std::make_shared<AnimationClip>();
        newClip->name = "New Animation Clip";
        newClip->duration = 5.0f;
        timeline.SetClip(newClip);
    }

    ImGui::Separator();
    ImGui::Text("Animation Tracks");

    auto clip = timeline.GetCurrentClip();
    if (clip) {
        for (size_t trackIdx = 0; trackIdx < clip->tracks.size(); ++trackIdx) {
            auto& track = clip->tracks[trackIdx];
            ImGui::PushID(static_cast<int>(trackIdx));
            if (ImGui::TreeNode(track.targetNodeName.c_str())) {
                if (ImGui::TreeNode("Position Keyframes")) {
                    for (size_t kIdx = 0; kIdx < track.positionKeys.size(); ++kIdx) {
                        auto& k = track.positionKeys[kIdx];
                        ImGui::BulletText("t=%.2fs | Pos: (%.2f, %.2f, %.2f)", k.time, k.value.x, k.value.y, k.value.z);
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Rotation Keyframes")) {
                    for (size_t kIdx = 0; kIdx < track.rotationKeys.size(); ++kIdx) {
                        auto& k = track.rotationKeys[kIdx];
                        glm::vec3 euler = glm::degrees(glm::eulerAngles(k.value));
                        ImGui::BulletText("t=%.2fs | Rot: (%.1f°, %.1f°, %.1f°)", k.time, euler.x, euler.y, euler.z);
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Scale Keyframes")) {
                    for (size_t kIdx = 0; kIdx < track.scaleKeys.size(); ++kIdx) {
                        auto& k = track.scaleKeys[kIdx];
                        ImGui::BulletText("t=%.2fs | Scale: (%.2f, %.2f, %.2f)", k.time, k.value.x, k.value.y, k.value.z);
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    } else {
        ImGui::TextDisabled("No Animation Clip Loaded");
    }

    ImGui::End();
}

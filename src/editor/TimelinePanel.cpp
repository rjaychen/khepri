#include "TimelinePanel.h"

void TimelinePanel::RenderUI(Timeline& timeline) {
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
        timeline.SetCurrentTime(currentTime);
    }

    ImGui::Separator();
    ImGui::Text("Animation Tracks");

    auto clip = timeline.GetCurrentClip();
    if (clip) {
        for (const auto& track : clip->tracks) {
            if (ImGui::TreeNode(track.targetNodeName.c_str())) {
                ImGui::Text("Position Keys: %d", (int)track.positionKeys.size());
                ImGui::Text("Rotation Keys: %d", (int)track.rotationKeys.size());
                ImGui::Text("Scale Keys: %d", (int)track.scaleKeys.size());
                ImGui::TreePop();
            }
        }
    } else {
        ImGui::TextDisabled("No Animation Clip Loaded");
    }

    ImGui::End();
}

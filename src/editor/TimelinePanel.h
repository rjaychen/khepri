#pragma once

#include <imgui.h>
#include "../animation/Timeline.h"
#include "../scene/SceneNode.h"

class TimelinePanel {
public:
    TimelinePanel() = default;

    void RenderUI(Timeline& timeline, SceneNode* selectedNode = nullptr, SceneNode* rootSceneNode = nullptr);
};

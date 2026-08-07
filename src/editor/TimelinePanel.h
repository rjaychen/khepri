#pragma once

#include <imgui.h>
#include "../animation/Timeline.h"

class TimelinePanel {
public:
    TimelinePanel() = default;

    void RenderUI(Timeline& timeline);
};

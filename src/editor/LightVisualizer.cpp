#include "LightVisualizer.h"
#include "../scene/Camera.h"
#include "../scene/SceneNode.h"
#include "../scene/LightComponent.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace khepri {

namespace {

void BuildOrthonormalBasis(const glm::vec3& dir, glm::vec3& outU, glm::vec3& outV) {
    glm::vec3 w = (glm::length(dir) > 1e-4f) ? glm::normalize(dir) : glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 up = (std::abs(w.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    outU = glm::normalize(glm::cross(w, up));
    outV = glm::normalize(glm::cross(w, outU));
}

} // namespace

SpotConeGeometry LightVisualizer::CalculateSpotConeGeometry(
    const glm::vec3& worldPos,
    const glm::vec3& worldDir,
    float range,
    float innerAngleDeg,
    float outerAngleDeg,
    uint32_t circleSegments
) noexcept {
    SpotConeGeometry result{};
    result.apex = worldPos;

    float safeRange = std::max(0.05f, range);
    float safeInner = std::clamp(innerAngleDeg, 0.1f, 89.0f);
    float safeOuter = std::clamp(outerAngleDeg, safeInner, 89.5f);

    glm::vec3 normDir = (glm::length(worldDir) > 1e-4f) ? glm::normalize(worldDir) : glm::vec3(0.0f, -1.0f, 0.0f);
    result.baseCenter = worldPos + normDir * safeRange;

    glm::vec3 u, v;
    BuildOrthonormalBasis(normDir, u, v);

    float tanOuter = std::tan(glm::radians(safeOuter));
    float tanInner = std::tan(glm::radians(safeInner));

    result.outerRadius = safeRange * tanOuter;
    result.innerRadius = safeRange * tanInner;

    uint32_t segments = std::max(8u, circleSegments);
    result.outerCirclePoints.reserve(segments);
    result.innerCirclePoints.reserve(segments);

    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (uint32_t i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) * step;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        result.outerCirclePoints.push_back(result.baseCenter + (u * cosA + v * sinA) * result.outerRadius);
        result.innerCirclePoints.push_back(result.baseCenter + (u * cosA + v * sinA) * result.innerRadius);
    }

    // 4 cardinal generating lines from apex to outer circle
    result.apexGeneratingLines.reserve(4);
    result.apexGeneratingLines.push_back({ worldPos, result.baseCenter + u * result.outerRadius });
    result.apexGeneratingLines.push_back({ worldPos, result.baseCenter - u * result.outerRadius });
    result.apexGeneratingLines.push_back({ worldPos, result.baseCenter + v * result.outerRadius });
    result.apexGeneratingLines.push_back({ worldPos, result.baseCenter - v * result.outerRadius });

    // 4 cardinal inner generating lines (penumbra guide)
    result.innerGeneratingLines.reserve(4);
    result.innerGeneratingLines.push_back({ worldPos, result.baseCenter + u * result.innerRadius });
    result.innerGeneratingLines.push_back({ worldPos, result.baseCenter - u * result.innerRadius });
    result.innerGeneratingLines.push_back({ worldPos, result.baseCenter + v * result.innerRadius });
    result.innerGeneratingLines.push_back({ worldPos, result.baseCenter - v * result.innerRadius });

    // Central axis
    result.centerAxis = { worldPos, result.baseCenter };

    return result;
}

DirectionalRaysGeometry LightVisualizer::CalculateDirectionalRaysGeometry(
    const glm::vec3& worldPos,
    const glm::vec3& worldDir,
    float ringRadius,
    float rayLength,
    uint32_t numRays,
    uint32_t ringSegments
) noexcept {
    DirectionalRaysGeometry result{};
    result.origin = worldPos;

    glm::vec3 normDir = (glm::length(worldDir) > 1e-4f) ? glm::normalize(worldDir) : glm::vec3(0.0f, -1.0f, 0.0f);
    result.direction = normDir;

    float safeLength = std::max(0.2f, rayLength);
    float safeRadius = std::max(0.1f, ringRadius);

    glm::vec3 u, v;
    BuildOrthonormalBasis(normDir, u, v);

    // Central ray
    glm::vec3 centralEnd = worldPos + normDir * safeLength;
    result.centralRay = { worldPos, centralEnd };

    // Central arrowhead
    float arrowSize = safeLength * 0.18f;
    result.centralArrowHead.push_back({ centralEnd, centralEnd - normDir * arrowSize + u * (arrowSize * 0.45f) });
    result.centralArrowHead.push_back({ centralEnd, centralEnd - normDir * arrowSize - u * (arrowSize * 0.45f) });
    result.centralArrowHead.push_back({ centralEnd, centralEnd - normDir * arrowSize + v * (arrowSize * 0.45f) });
    result.centralArrowHead.push_back({ centralEnd, centralEnd - normDir * arrowSize - v * (arrowSize * 0.45f) });

    // Base ring points
    uint32_t rSegments = std::max(8u, ringSegments);
    result.baseRingPoints.reserve(rSegments);
    float ringStep = glm::two_pi<float>() / static_cast<float>(rSegments);
    for (uint32_t i = 0; i < rSegments; ++i) {
        float angle = static_cast<float>(i) * ringStep;
        result.baseRingPoints.push_back(worldPos + (u * std::cos(angle) + v * std::sin(angle)) * safeRadius);
    }

    // Parallel rays distributed around the ring
    uint32_t rays = std::max(3u, numRays);
    result.parallelRays.reserve(rays);
    float rayStep = glm::two_pi<float>() / static_cast<float>(rays);

    for (uint32_t i = 0; i < rays; ++i) {
        float angle = static_cast<float>(i) * rayStep;
        glm::vec3 rayStart = worldPos + (u * std::cos(angle) + v * std::sin(angle)) * safeRadius;
        glm::vec3 rayEnd = rayStart + normDir * safeLength;

        result.parallelRays.push_back({ rayStart, rayEnd });

        // Arrowhead for each parallel ray
        result.rayArrowHeads.push_back({ rayEnd, rayEnd - normDir * arrowSize + u * (arrowSize * 0.35f) });
        result.rayArrowHeads.push_back({ rayEnd, rayEnd - normDir * arrowSize - u * (arrowSize * 0.35f) });
    }

    return result;
}

PointLightRingsGeometry LightVisualizer::CalculatePointLightRingsGeometry(
    const glm::vec3& worldPos,
    float range,
    uint32_t segments
) noexcept {
    PointLightRingsGeometry result{};
    result.center = worldPos;
    result.radius = std::max(0.05f, range);

    uint32_t segCount = std::max(8u, segments);
    result.xyRingPoints.reserve(segCount);
    result.xzRingPoints.reserve(segCount);
    result.yzRingPoints.reserve(segCount);

    float step = glm::two_pi<float>() / static_cast<float>(segCount);
    for (uint32_t i = 0; i < segCount; ++i) {
        float angle = static_cast<float>(i) * step;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // XY plane ring
        result.xyRingPoints.push_back(worldPos + glm::vec3(cosA * result.radius, sinA * result.radius, 0.0f));
        // XZ plane ring (horizontal)
        result.xzRingPoints.push_back(worldPos + glm::vec3(cosA * result.radius, 0.0f, sinA * result.radius));
        // YZ plane ring (vertical)
        result.yzRingPoints.push_back(worldPos + glm::vec3(0.0f, cosA * result.radius, sinA * result.radius));
    }

    return result;
}

bool LightVisualizer::ProjectLineSegment(
    const glm::vec3& p1World,
    const glm::vec3& p2World,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    glm::vec2& outScreen1,
    glm::vec2& outScreen2
) noexcept {
    glm::vec4 c1 = viewProj * glm::vec4(p1World, 1.0f);
    glm::vec4 c2 = viewProj * glm::vec4(p2World, 1.0f);

    constexpr float nearW = 0.001f;

    // Both vertices behind near plane -> discard
    if (c1.w <= nearW && c2.w <= nearW) {
        return false;
    }

    glm::vec3 finalP1 = p1World;
    glm::vec3 finalP2 = p2World;

    // Clip against near plane if one point is behind camera
    if (c1.w <= nearW) {
        float t = (nearW - c1.w) / (c2.w - c1.w);
        finalP1 = p1World + t * (p2World - p1World);
        c1 = viewProj * glm::vec4(finalP1, 1.0f);
    } else if (c2.w <= nearW) {
        float t = (nearW - c2.w) / (c1.w - c2.w);
        finalP2 = p2World + t * (p1World - p2World);
        c2 = viewProj * glm::vec4(finalP2, 1.0f);
    }

    if (c1.w <= 0.0f || c2.w <= 0.0f) {
        return false;
    }

    glm::vec2 ndc1(c1.x / c1.w, c1.y / c1.w);
    glm::vec2 ndc2(c2.x / c2.w, c2.y / c2.w);

    outScreen1 = viewportPos + glm::vec2(ndc1.x * 0.5f + 0.5f, ndc1.y * 0.5f + 0.5f) * viewportSize;
    outScreen2 = viewportPos + glm::vec2(ndc2.x * 0.5f + 0.5f, ndc2.y * 0.5f + 0.5f) * viewportSize;

    return true;
}

void LightVisualizer::Draw3DLine(
    ImDrawList* drawList,
    const glm::vec3& p1,
    const glm::vec3& p2,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    ImU32 color,
    float thickness
) {
    glm::vec2 s1, s2;
    if (ProjectLineSegment(p1, p2, viewProj, viewportPos, viewportSize, s1, s2)) {
        // High-contrast background halo line
        drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), IM_COL32(10, 10, 10, 140), thickness + 1.6f);
        // Foreground light-tinted line
        drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), color, thickness);
    }
}

void LightVisualizer::Draw3DLoop(
    ImDrawList* drawList,
    const std::vector<glm::vec3>& loopPoints,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    ImU32 color,
    float thickness
) {
    if (loopPoints.size() < 3) return;

    size_t count = loopPoints.size();
    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % count;
        Draw3DLine(drawList, loopPoints[i], loopPoints[next], viewProj, viewportPos, viewportSize, color, thickness);
    }
}

bool LightVisualizer::ProjectWorldToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& viewProj,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    glm::vec2& outScreen
) noexcept {
    glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
    if (clipPos.w <= 0.001f) {
        return false;
    }

    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    if (std::abs(ndc.x) > 1.2f || std::abs(ndc.y) > 1.2f) {
        return false;
    }

    outScreen.x = viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
    outScreen.y = viewportPos.y + (ndc.y * 0.5f + 0.5f) * viewportSize.y;
    return true;
}

void LightVisualizer::DrawLightIcon(
    ImDrawList* drawList,
    const glm::vec2& screenCenter,
    LightType type,
    const glm::vec3& lightColor,
    bool isSelected,
    bool isHovered
) {
    if (!drawList) return;

    constexpr float badgeRadius = 16.0f;
    const ImVec2 center(screenCenter.x, screenCenter.y);

    // 1. Dark Backdrop Disc with border for high contrast
    drawList->AddCircleFilled(center, badgeRadius, IM_COL32(22, 24, 30, 215), 24);
    drawList->AddCircle(center, badgeRadius, IM_COL32(10, 10, 15, 230), 24, 1.5f);

    // Compute brightened light color for contrast
    int r = static_cast<int>(std::clamp(lightColor.r * 255.0f, 60.0f, 255.0f));
    int g = static_cast<int>(std::clamp(lightColor.g * 255.0f, 60.0f, 255.0f));
    int b = static_cast<int>(std::clamp(lightColor.b * 255.0f, 60.0f, 255.0f));
    ImU32 glyphColor = IM_COL32(r, g, b, 255);

    // 2. Selection & Hover Rings
    if (isSelected) {
        drawList->AddCircle(center, badgeRadius + 3.0f, IM_COL32(255, 205, 45, 255), 24, 2.0f);
    }
    if (isHovered) {
        drawList->AddCircle(center, badgeRadius + (isSelected ? 5.0f : 2.5f), IM_COL32(255, 255, 255, 190), 24, 1.5f);
    }

    // 3. Draw Type-Specific Vector Glyphs
    switch (type) {
        case LightType::Directional: {
            // Sun: Filled central disk + 8 radiating rays
            constexpr float sunCoreRadius = 4.5f;
            drawList->AddCircleFilled(center, sunCoreRadius, glyphColor, 16);
            drawList->AddCircle(center, sunCoreRadius, IM_COL32(15, 15, 20, 200), 16, 1.0f);

            constexpr float rayInner = 6.5f;
            constexpr float rayOuter = 11.5f;
            constexpr float step = glm::two_pi<float>() / 8.0f;
            for (int i = 0; i < 8; ++i) {
                float angle = static_cast<float>(i) * step;
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);
                ImVec2 p1(screenCenter.x + cosA * rayInner, screenCenter.y + sinA * rayInner);
                ImVec2 p2(screenCenter.x + cosA * rayOuter, screenCenter.y + sinA * rayOuter);
                drawList->AddLine(p1, p2, glyphColor, 1.6f);
            }
            break;
        }

        case LightType::Point: {
            // Bulb: Spherical top bulb + base filament / rays
            ImVec2 bulbCenter(screenCenter.x, screenCenter.y - 2.0f);
            constexpr float bulbRadius = 5.5f;
            drawList->AddCircleFilled(bulbCenter, bulbRadius, glyphColor, 16);
            drawList->AddCircle(bulbCenter, bulbRadius, IM_COL32(15, 15, 20, 200), 16, 1.0f);

            // Bulb socket / base
            ImVec2 baseTopL(screenCenter.x - 3.0f, screenCenter.y + 4.0f);
            ImVec2 baseTopR(screenCenter.x + 3.0f, screenCenter.y + 4.0f);
            ImVec2 baseBotL(screenCenter.x - 2.0f, screenCenter.y + 6.5f);
            ImVec2 baseBotR(screenCenter.x + 2.0f, screenCenter.y + 6.5f);
            drawList->AddLine(baseTopL, baseTopR, IM_COL32(200, 200, 210, 240), 1.6f);
            drawList->AddLine(baseBotL, baseBotR, IM_COL32(170, 170, 180, 240), 1.6f);

            // Inner glowing dot
            drawList->AddCircleFilled(bulbCenter, 2.0f, IM_COL32(255, 255, 255, 240), 8);
            break;
        }

        case LightType::Spot: {
            // Spotlight / Flashlight cone: Apex emitter at top, diverging beam downwards
            ImVec2 apex(screenCenter.x, screenCenter.y - 6.0f);
            ImVec2 leftBeam(screenCenter.x - 7.5f, screenCenter.y + 6.5f);
            ImVec2 rightBeam(screenCenter.x + 7.5f, screenCenter.y + 6.5f);

            // Subtle semi-transparent beam fill
            drawList->AddTriangleFilled(apex, leftBeam, rightBeam, IM_COL32(r, g, b, 70));

            // Cone beam outline
            drawList->AddLine(apex, leftBeam, glyphColor, 1.6f);
            drawList->AddLine(apex, rightBeam, glyphColor, 1.6f);
            drawList->AddLine(leftBeam, rightBeam, glyphColor, 1.6f);

            // Apex bulb dot
            drawList->AddCircleFilled(apex, 2.5f, glyphColor, 10);
            drawList->AddCircleFilled(apex, 1.2f, IM_COL32(255, 255, 255, 255), 8);
            break;
        }
    }
}

void LightVisualizer::RenderSceneLights(
    ImDrawList* drawList,
    const Camera& camera,
    const SceneNode* rootNode,
    const SceneNode* selectedNode,
    LightHelperDisplayMode displayMode,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    const glm::vec2& mousePos,
    const SceneNode** outHoveredLightNode
) {
    if (!rootNode || displayMode == LightHelperDisplayMode::Hidden || !drawList) {
        if (outHoveredLightNode) *outHoveredLightNode = nullptr;
        return;
    }

    if (outHoveredLightNode) {
        *outHoveredLightNode = nullptr;
    }

    struct LightEntry {
        const SceneNode* node = nullptr;
        glm::mat4 worldTransform{1.0f};
        glm::vec2 screenPos{0.0f};
        bool inView = false;
    };

    std::vector<LightEntry> lightEntries;
    lightEntries.reserve(16);

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();

    std::function<void(const SceneNode*, const glm::mat4&)> traverse =
        [&](const SceneNode* node, const glm::mat4& parentTransform) {
            if (!node || !node->visible) return;

            glm::mat4 worldTransform = parentTransform * node->GetLocalTransform();

            if (node->lightComponent) {
                LightEntry entry;
                entry.node = node;
                entry.worldTransform = worldTransform;
                glm::vec3 worldPos = glm::vec3(worldTransform[3]);
                entry.inView = ProjectWorldToScreen(worldPos, viewProj, viewportPos, viewportSize, entry.screenPos);
                lightEntries.push_back(entry);
            }

            for (const auto& child : node->GetChildren()) {
                traverse(child.get(), worldTransform);
            }
        };

    traverse(rootNode, glm::mat4(1.0f));

    // Find best hovered light icon (18px hit radius)
    const SceneNode* hoveredNode = nullptr;
    float closestHoverDist = 18.0f;

    for (const auto& entry : lightEntries) {
        if (entry.inView) {
            float dist = glm::distance(mousePos, entry.screenPos);
            if (dist <= closestHoverDist) {
                closestHoverDist = dist;
                hoveredNode = entry.node;
            }
        }
    }

    if (outHoveredLightNode) {
        *outHoveredLightNode = hoveredNode;
    }

    // Render all light helpers and icons
    for (const auto& entry : lightEntries) {
        bool isSelected = (entry.node == selectedNode);
        bool isHovered = (entry.node == hoveredNode);
        bool drawGuides = (displayMode == LightHelperDisplayMode::All) ||
                          (displayMode == LightHelperDisplayMode::Selected && isSelected);

        RenderSingleLightHelper(
            drawList,
            camera,
            entry.node,
            entry.worldTransform,
            viewportPos,
            viewportSize,
            isSelected,
            drawGuides,
            isHovered
        );
    }

    // Tooltip for hovered light icon
    if (hoveredNode && hoveredNode->lightComponent) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Light: %s", hoveredNode->name.c_str());
        const char* typeStr = (hoveredNode->lightComponent->type == LightType::Directional) ? "Directional (Sun)" :
                              (hoveredNode->lightComponent->type == LightType::Point) ? "Point Light" : "Spot Light";
        ImGui::TextDisabled("Type: %s", typeStr);
        ImGui::Text("Intensity: %.2f", hoveredNode->lightComponent->intensity);
        ImGui::EndTooltip();
    }
}

void LightVisualizer::RenderSingleLightHelper(
    ImDrawList* drawList,
    const Camera& camera,
    const SceneNode* node,
    const glm::mat4& worldTransform,
    const glm::vec2& viewportPos,
    const glm::vec2& viewportSize,
    bool isSelected,
    bool drawWireframeGuides,
    bool isHovered
) {
    if (!node || !node->lightComponent || !drawList) return;

    const auto& light = *node->lightComponent;
    glm::vec3 worldPos = glm::vec3(worldTransform[3]);

    glm::mat3 rotMat(worldTransform);
    if (glm::length(rotMat[0]) > 1e-5f) rotMat[0] = glm::normalize(rotMat[0]);
    if (glm::length(rotMat[1]) > 1e-5f) rotMat[1] = glm::normalize(rotMat[1]);
    if (glm::length(rotMat[2]) > 1e-5f) rotMat[2] = glm::normalize(rotMat[2]);

    glm::vec3 worldDir = rotMat * light.direction;
    if (glm::length(worldDir) > 1e-4f) {
        worldDir = glm::normalize(worldDir);
    } else {
        worldDir = glm::vec3(0.0f, -1.0f, 0.0f);
    }

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();

    if (drawWireframeGuides) {
        // Color computation: tint with light color, brightened for readability
        int r = static_cast<int>(std::clamp(light.color.r * 255.0f, 50.0f, 255.0f));
        int g = static_cast<int>(std::clamp(light.color.g * 255.0f, 50.0f, 255.0f));
        int b = static_cast<int>(std::clamp(light.color.b * 255.0f, 50.0f, 255.0f));
        int alphaPrimary = isSelected ? 255 : 175;
        int alphaSecondary = isSelected ? 180 : 110;

        ImU32 primaryColor = IM_COL32(r, g, b, alphaPrimary);
        ImU32 secondaryColor = IM_COL32(r, g, b, alphaSecondary);

        float lineThickness = isSelected ? 2.0f : 1.3f;
        float guideThickness = isSelected ? 1.4f : 1.0f;

        switch (light.type) {
            case LightType::Directional: {
                auto dirGeom = CalculateDirectionalRaysGeometry(worldPos, worldDir, 0.85f, 2.5f, 5, 24);

                // Base ring
                Draw3DLoop(drawList, dirGeom.baseRingPoints, viewProj, viewportPos, viewportSize, secondaryColor, guideThickness);

                // Central ray and arrow
                Draw3DLine(drawList, dirGeom.centralRay.start, dirGeom.centralRay.end, viewProj, viewportPos, viewportSize, primaryColor, lineThickness + 0.5f);
                for (const auto& arrow : dirGeom.centralArrowHead) {
                    Draw3DLine(drawList, arrow.start, arrow.end, viewProj, viewportPos, viewportSize, primaryColor, lineThickness + 0.5f);
                }

                // Parallel rays and arrows
                for (const auto& ray : dirGeom.parallelRays) {
                    Draw3DLine(drawList, ray.start, ray.end, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                }
                for (const auto& arrow : dirGeom.rayArrowHeads) {
                    Draw3DLine(drawList, arrow.start, arrow.end, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                }
                break;
            }

            case LightType::Spot: {
                auto spotGeom = CalculateSpotConeGeometry(
                    worldPos,
                    worldDir,
                    light.range,
                    light.innerCutoffAngle,
                    light.outerCutoffAngle,
                    32
                );

                // Center axis line
                Draw3DLine(drawList, spotGeom.centerAxis.start, spotGeom.centerAxis.end, viewProj, viewportPos, viewportSize, secondaryColor, guideThickness);

                // Outer cone circle (full FOV)
                Draw3DLoop(drawList, spotGeom.outerCirclePoints, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);

                // Outer generating lines from apex
                for (const auto& line : spotGeom.apexGeneratingLines) {
                    Draw3DLine(drawList, line.start, line.end, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                }

                // Inner cone circle (hotspot)
                if (light.innerCutoffAngle < light.outerCutoffAngle - 0.5f) {
                    Draw3DLoop(drawList, spotGeom.innerCirclePoints, viewProj, viewportPos, viewportSize, secondaryColor, guideThickness);
                    for (const auto& line : spotGeom.innerGeneratingLines) {
                        Draw3DLine(drawList, line.start, line.end, viewProj, viewportPos, viewportSize, secondaryColor, guideThickness * 0.8f);
                    }
                }
                break;
            }

            case LightType::Point: {
                auto pointGeom = CalculatePointLightRingsGeometry(worldPos, light.range, 36);

                // 3 orthogonal attenuation range rings
                Draw3DLoop(drawList, pointGeom.xyRingPoints, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                Draw3DLoop(drawList, pointGeom.xzRingPoints, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                Draw3DLoop(drawList, pointGeom.yzRingPoints, viewProj, viewportPos, viewportSize, primaryColor, lineThickness);
                break;
            }
        }
    }

    // Draw procedural 2D billboard icon
    glm::vec2 screenPos{0.0f};
    if (ProjectWorldToScreen(worldPos, viewProj, viewportPos, viewportSize, screenPos)) {
        DrawLightIcon(drawList, screenPos, light.type, light.color, isSelected, isHovered);
    }
}

} // namespace khepri

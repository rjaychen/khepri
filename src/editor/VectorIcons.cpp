#include "VectorIcons.h"
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>

namespace khepri::ui {

void VectorIcons::Draw(ImDrawList* drawList, VectorIconType type, const ImVec2& center, float size, ImU32 color, bool state) {
    if (!drawList) drawList = ImGui::GetWindowDrawList();

    switch (type) {
        case VectorIconType::Mesh:        DrawMesh(drawList, center, size, color); break;
        case VectorIconType::Sun:         DrawSun(drawList, center, size, color); break;
        case VectorIconType::PointLight:  DrawPointLight(drawList, center, size, color); break;
        case VectorIconType::SpotLight:   DrawSpotLight(drawList, center, size, color); break;
        case VectorIconType::Folder:      DrawFolder(drawList, center, size, color, false); break;
        case VectorIconType::FolderOpen:  DrawFolder(drawList, center, size, color, true); break;
        case VectorIconType::File:        DrawFile(drawList, center, size, color); break;
        case VectorIconType::Eye:         DrawEye(drawList, center, size, color, true); break;
        case VectorIconType::EyeSlash:    DrawEye(drawList, center, size, color, false); break;
        case VectorIconType::Lock:        DrawLock(drawList, center, size, color, true); break;
        case VectorIconType::Unlock:      DrawLock(drawList, center, size, color, false); break;
        case VectorIconType::Camera:      DrawCamera(drawList, center, size, color); break;
        case VectorIconType::Material:    DrawMaterial(drawList, center, size, color); break;
        case VectorIconType::Texture:     DrawTexture(drawList, center, size, color); break;
        case VectorIconType::Shader:      DrawShader(drawList, center, size, color); break;
        case VectorIconType::Scene:       DrawScene(drawList, center, size, color); break;
        case VectorIconType::Search:      DrawSearch(drawList, center, size, color); break;
        case VectorIconType::Grid:        DrawGrid(drawList, center, size, color); break;
        case VectorIconType::List:        DrawList(drawList, center, size, color); break;
        case VectorIconType::Plus:        DrawPlus(drawList, center, size, color); break;
        case VectorIconType::Trash:       DrawTrash(drawList, center, size, color); break;
        case VectorIconType::Node:        DrawNode(drawList, center, size, color); break;
    }
    (void)state;
}

void VectorIcons::DrawMesh(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float r = size * 0.46f;
    float h = r * 0.55f;
    float w = r * 0.866f;

    ImVec2 top(center.x, center.y - r);
    ImVec2 topR(center.x + w, center.y - h);
    ImVec2 botR(center.x + w, center.y + h);
    ImVec2 bot(center.x, center.y + r);
    ImVec2 botL(center.x - w, center.y + h);
    ImVec2 topL(center.x - w, center.y - h);
    ImVec2 mid(center.x, center.y);

    // Extract RGB to make shaded faces
    ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
    ImU32 topFaceCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * 1.0f, col.y * 1.0f, col.z * 1.0f, col.w * 0.45f));
    ImU32 leftFaceCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * 0.8f, col.y * 0.8f, col.z * 0.8f, col.w * 0.35f));
    ImU32 rightFaceCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * 0.6f, col.y * 0.6f, col.z * 0.6f, col.w * 0.25f));

    // Faces
    drawList->AddQuadFilled(top, topR, mid, topL, topFaceCol);
    drawList->AddQuadFilled(topL, mid, bot, botL, leftFaceCol);
    drawList->AddQuadFilled(mid, topR, botR, bot, rightFaceCol);

    // Outlines
    drawList->AddLine(top, topR, color, 1.2f);
    drawList->AddLine(topR, botR, color, 1.2f);
    drawList->AddLine(botR, bot, color, 1.2f);
    drawList->AddLine(bot, botL, color, 1.2f);
    drawList->AddLine(botL, topL, color, 1.2f);
    drawList->AddLine(topL, top, color, 1.2f);

    drawList->AddLine(mid, top, color, 1.2f);
    drawList->AddLine(mid, botL, color, 1.2f);
    drawList->AddLine(mid, botR, color, 1.2f);
}

void VectorIcons::DrawSun(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float coreR = size * 0.20f;
    drawList->AddCircleFilled(center, coreR, color, 16);

    float rayIn = size * 0.28f;
    float rayOut = size * 0.44f;
    for (int i = 0; i < 8; ++i) {
        float angle = i * (3.14159265f / 4.0f);
        float ca = std::cos(angle);
        float sa = std::sin(angle);
        drawList->AddLine(ImVec2(center.x + ca * rayIn, center.y + sa * rayIn),
                          ImVec2(center.x + ca * rayOut, center.y + sa * rayOut),
                          color, 1.4f);
    }
}

void VectorIcons::DrawPointLight(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    ImVec2 bulbCenter(center.x, center.y - size * 0.08f);
    float bulbRadius = size * 0.28f;

    // Glowing bulb dome
    drawList->AddCircleFilled(bulbCenter, bulbRadius, color, 16);
    drawList->AddCircleFilled(bulbCenter, bulbRadius * 0.45f, IM_COL32(255, 255, 255, 230), 10);

    // Socket base lines
    float baseHalfW = size * 0.16f;
    float baseY1 = center.y + size * 0.22f;
    float baseY2 = center.y + size * 0.36f;
    drawList->AddLine(ImVec2(center.x - baseHalfW, baseY1), ImVec2(center.x + baseHalfW, baseY1), color, 1.4f);
    drawList->AddLine(ImVec2(center.x - baseHalfW * 0.6f, baseY2), ImVec2(center.x + baseHalfW * 0.6f, baseY2), color, 1.4f);
}

void VectorIcons::DrawSpotLight(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    ImVec2 apex(center.x, center.y - size * 0.35f);
    ImVec2 leftBeam(center.x - size * 0.40f, center.y + size * 0.35f);
    ImVec2 rightBeam(center.x + size * 0.40f, center.y + size * 0.35f);

    ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * 0.35f));

    drawList->AddTriangleFilled(apex, leftBeam, rightBeam, fillCol);
    drawList->AddLine(apex, leftBeam, color, 1.3f);
    drawList->AddLine(apex, rightBeam, color, 1.3f);
    drawList->AddLine(leftBeam, rightBeam, color, 1.3f);

    drawList->AddCircleFilled(apex, size * 0.12f, color, 12);
}

void VectorIcons::DrawFolder(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool open) {
    float hw = size * 0.42f;
    float hh = size * 0.32f;

    ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * 0.30f));

    // Back folder plate with tab
    ImVec2 p0(center.x - hw, center.y - hh);
    ImVec2 pTab(center.x - hw * 0.2f, center.y - hh);
    ImVec2 pTabR(center.x, center.y - hh + size * 0.12f);
    ImVec2 pTopR(center.x + hw, center.y - hh + size * 0.12f);
    ImVec2 pBotR(center.x + hw, center.y + hh);
    ImVec2 pBotL(center.x - hw, center.y + hh);

    drawList->AddQuadFilled(p0, pTopR, pBotR, pBotL, fillCol);
    drawList->AddLine(p0, pTab, color, 1.2f);
    drawList->AddLine(pTab, pTabR, color, 1.2f);
    drawList->AddLine(pTabR, pTopR, color, 1.2f);
    drawList->AddLine(pTopR, pBotR, color, 1.2f);
    drawList->AddLine(pBotR, pBotL, color, 1.2f);
    drawList->AddLine(pBotL, p0, color, 1.2f);

    // Front flap
    if (open) {
        ImVec2 flapTL(center.x - hw * 0.9f, center.y - hh * 0.2f);
        ImVec2 flapTR(center.x + hw * 1.1f, center.y - hh * 0.2f);
        drawList->AddQuadFilled(flapTL, flapTR, pBotR, pBotL, fillCol);
        drawList->AddLine(flapTL, flapTR, color, 1.2f);
        drawList->AddLine(flapTR, pBotR, color, 1.2f);
        drawList->AddLine(pBotL, flapTL, color, 1.2f);
    }
}

void VectorIcons::DrawFile(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float hw = size * 0.32f;
    float hh = size * 0.42f;
    float fold = size * 0.22f;

    ImVec2 p0(center.x - hw, center.y - hh);
    ImVec2 pFoldL(center.x + hw - fold, center.y - hh);
    ImVec2 pFoldB(center.x + hw, center.y - hh + fold);
    ImVec2 pBotR(center.x + hw, center.y + hh);
    ImVec2 pBotL(center.x - hw, center.y + hh);

    ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * 0.25f));

    drawList->AddQuadFilled(p0, pFoldL, pBotR, pBotL, fillCol);

    drawList->AddLine(p0, pFoldL, color, 1.2f);
    drawList->AddLine(pFoldL, pFoldB, color, 1.2f);
    drawList->AddLine(pFoldB, pBotR, color, 1.2f);
    drawList->AddLine(pBotR, pBotL, color, 1.2f);
    drawList->AddLine(pBotL, p0, color, 1.2f);

    // Fold inner corner
    drawList->AddLine(pFoldL, ImVec2(pFoldL.x, pFoldB.y), color, 1.0f);
    drawList->AddLine(ImVec2(pFoldL.x, pFoldB.y), pFoldB, color, 1.0f);
}

void VectorIcons::DrawEye(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool visible) {
    float hw = size * 0.44f;
    float hh = size * 0.26f;

    // Curved almond contour using quadratic bezier curves
    ImVec2 left(center.x - hw, center.y);
    ImVec2 right(center.x + hw, center.y);
    ImVec2 topCtrl(center.x, center.y - hh * 1.5f);
    ImVec2 botCtrl(center.x, center.y + hh * 1.5f);

    drawList->AddBezierQuadratic(left, topCtrl, right, color, 1.4f);
    drawList->AddBezierQuadratic(left, botCtrl, right, color, 1.4f);

    if (visible) {
        // Pupil
        drawList->AddCircleFilled(center, size * 0.16f, color, 16);
        drawList->AddCircleFilled(ImVec2(center.x + 1.0f, center.y - 1.0f), size * 0.06f, IM_COL32(255, 255, 255, 240), 8);
    } else {
        // Diagonal slash stroke across eye
        float slash = size * 0.46f;
        drawList->AddLine(ImVec2(center.x - slash, center.y - slash * 0.8f),
                          ImVec2(center.x + slash, center.y + slash * 0.8f),
                          color, 1.6f);
    }
}

void VectorIcons::DrawLock(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color, bool locked) {
    float bodyW = size * 0.64f;
    float bodyH = size * 0.44f;
    ImVec2 bodyMin(center.x - bodyW * 0.5f, center.y);
    ImVec2 bodyMax(center.x + bodyW * 0.5f, center.y + bodyH);

    // Body
    drawList->AddRectFilled(bodyMin, bodyMax, color, 2.5f);

    // Keyhole dot
    ImVec2 keyHole(center.x, center.y + bodyH * 0.45f);
    drawList->AddCircleFilled(keyHole, size * 0.08f, IM_COL32(20, 24, 30, 255), 8);

    // Shackle arc
    float shackleR = size * 0.20f;
    float shackleOffset = locked ? 0.0f : -size * 0.16f;
    ImVec2 shackleCenter(center.x + shackleOffset, center.y);

    drawList->AddBezierQuadratic(ImVec2(shackleCenter.x - shackleR, shackleCenter.y),
                                 ImVec2(shackleCenter.x, shackleCenter.y - size * 0.42f),
                                 ImVec2(shackleCenter.x + shackleR, shackleCenter.y),
                                 color, 1.6f);
}

void VectorIcons::DrawCamera(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float hw = size * 0.40f;
    float hh = size * 0.28f;
    ImVec2 bodyMin(center.x - hw, center.y - hh + size * 0.06f);
    ImVec2 bodyMax(center.x + hw, center.y + hh);

    drawList->AddRect(bodyMin, bodyMax, color, 3.0f, 0, 1.3f);
    drawList->AddCircle(ImVec2(center.x, center.y + size * 0.04f), size * 0.18f, color, 16, 1.3f);
    // Viewfinder bump
    drawList->AddRectFilled(ImVec2(center.x - size * 0.16f, center.y - hh - size * 0.08f),
                            ImVec2(center.x + size * 0.06f, bodyMin.y), color, 1.5f);
}

void VectorIcons::DrawMaterial(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float r = size * 0.38f;
    drawList->AddCircle(center, r, color, 20, 1.3f);

    // Pigment dots
    float dotR = size * 0.07f;
    drawList->AddCircleFilled(ImVec2(center.x - r * 0.45f, center.y - r * 0.3f), dotR, color, 8);
    drawList->AddCircleFilled(ImVec2(center.x, center.y - r * 0.5f), dotR, color, 8);
    drawList->AddCircleFilled(ImVec2(center.x + r * 0.45f, center.y - r * 0.3f), dotR, color, 8);
    // Thumb hole
    drawList->AddCircleFilled(ImVec2(center.x + r * 0.35f, center.y + r * 0.35f), dotR * 1.3f, IM_COL32(18, 22, 28, 255), 8);
}

void VectorIcons::DrawTexture(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float hw = size * 0.40f;
    float hh = size * 0.34f;
    ImVec2 minPos(center.x - hw, center.y - hh);
    ImVec2 maxPos(center.x + hw, center.y + hh);

    drawList->AddRect(minPos, maxPos, color, 2.5f, 0, 1.3f);

    // Mountain triangles
    ImVec2 m1(minPos.x + 3.0f, maxPos.y - 2.0f);
    ImVec2 m2(center.x - hw * 0.1f, center.y - hh * 0.1f);
    ImVec2 m3(center.x + hw * 0.4f, maxPos.y - 2.0f);
    drawList->AddTriangleFilled(m1, m2, m3, color);

    // Sun dot
    drawList->AddCircleFilled(ImVec2(maxPos.x - size * 0.22f, minPos.y + size * 0.22f), size * 0.08f, color, 8);
}

void VectorIcons::DrawShader(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float s = size * 0.44f;
    ImVec2 p0(center.x + s * 0.1f, center.y - s);
    ImVec2 p1(center.x - s * 0.6f, center.y + s * 0.05f);
    ImVec2 p2(center.x - s * 0.05f, center.y + s * 0.05f);
    ImVec2 p3(center.x - s * 0.2f, center.y + s);
    ImVec2 p4(center.x + s * 0.6f, center.y - s * 0.15f);
    ImVec2 p5(center.x + s * 0.05f, center.y - s * 0.15f);

    drawList->AddTriangleFilled(p0, p1, p2, color);
    drawList->AddTriangleFilled(p2, p3, p4, color);
    drawList->AddTriangleFilled(p2, p4, p5, color);
}

void VectorIcons::DrawScene(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float r = size * 0.40f;
    drawList->AddCircle(center, r, color, 20, 1.3f);
    drawList->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), color, 1.1f);
    drawList->AddEllipse(center, ImVec2(r * 0.45f, r), color, 0.0f, 16, 1.1f);
}

void VectorIcons::DrawSearch(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float r = size * 0.25f;
    ImVec2 glassCenter(center.x - size * 0.08f, center.y - size * 0.08f);
    drawList->AddCircle(glassCenter, r, color, 16, 1.4f);

    float cos45 = 0.7071f;
    ImVec2 hStart(glassCenter.x + r * cos45, glassCenter.y + r * cos45);
    ImVec2 hEnd(center.x + size * 0.42f, center.y + size * 0.42f);
    drawList->AddLine(hStart, hEnd, color, 2.0f);
}

void VectorIcons::DrawGrid(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float q = size * 0.32f;
    float gap = size * 0.08f;
    drawList->AddRectFilled(ImVec2(center.x - q - gap * 0.5f, center.y - q - gap * 0.5f),
                            ImVec2(center.x - gap * 0.5f, center.y - gap * 0.5f), color, 1.5f);
    drawList->AddRectFilled(ImVec2(center.x + gap * 0.5f, center.y - q - gap * 0.5f),
                            ImVec2(center.x + q + gap * 0.5f, center.y - gap * 0.5f), color, 1.5f);
    drawList->AddRectFilled(ImVec2(center.x - q - gap * 0.5f, center.y + gap * 0.5f),
                            ImVec2(center.x - gap * 0.5f, center.y + q + gap * 0.5f), color, 1.5f);
    drawList->AddRectFilled(ImVec2(center.x + gap * 0.5f, center.y + gap * 0.5f),
                            ImVec2(center.x + q + gap * 0.5f, center.y + q + gap * 0.5f), color, 1.5f);
}

void VectorIcons::DrawList(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float hw = size * 0.40f;
    float dotR = size * 0.07f;
    float ys[3] = { center.y - size * 0.26f, center.y, center.y + size * 0.26f };

    for (int i = 0; i < 3; ++i) {
        drawList->AddCircleFilled(ImVec2(center.x - hw + dotR, ys[i]), dotR, color, 8);
        drawList->AddLine(ImVec2(center.x - hw + dotR * 3.5f, ys[i]),
                          ImVec2(center.x + hw, ys[i]), color, 1.4f);
    }
}

void VectorIcons::DrawPlus(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float h = size * 0.36f;
    drawList->AddLine(ImVec2(center.x - h, center.y), ImVec2(center.x + h, center.y), color, 1.8f);
    drawList->AddLine(ImVec2(center.x, center.y - h), ImVec2(center.x, center.y + h), color, 1.8f);
}

void VectorIcons::DrawTrash(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float hw = size * 0.30f;
    float hh = size * 0.36f;
    ImVec2 topL(center.x - hw, center.y - hh * 0.4f);
    ImVec2 topR(center.x + hw, center.y - hh * 0.4f);
    ImVec2 botL(center.x - hw * 0.75f, center.y + hh);
    ImVec2 botR(center.x + hw * 0.75f, center.y + hh);

    drawList->AddLine(topL, topR, color, 1.3f);
    drawList->AddLine(topR, botR, color, 1.3f);
    drawList->AddLine(botR, botL, color, 1.3f);
    drawList->AddLine(botL, topL, color, 1.3f);

    // Lid bar
    drawList->AddLine(ImVec2(center.x - hw * 1.2f, center.y - hh * 0.55f),
                      ImVec2(center.x + hw * 1.2f, center.y - hh * 0.55f), color, 1.4f);
}

void VectorIcons::DrawNode(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color) {
    float r = size * 0.38f;
    drawList->AddCircle(center, r, color, 16, 1.4f);
    drawList->AddCircleFilled(center, r * 0.45f, color, 12);
}

void VectorIcons::RenderInline(VectorIconType type, float size, const ImVec4& color, bool state) {
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center(cursor.x + size * 0.5f, cursor.y + size * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    Draw(drawList, type, center, size, ImGui::GetColorU32(color), state);
    ImGui::Dummy(ImVec2(size, size));
}

bool VectorIcons::IconButton(const char* strId, VectorIconType type, bool active, const char* tooltip, const ImVec2& size) {
    ImGui::PushID(strId);

    ImVec4 bgCol = active ? ImVec4(0.231f, 0.510f, 0.965f, 0.30f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, bgCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.25f, 0.35f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.231f, 0.510f, 0.965f, 0.60f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::Button("##btn", size);
    bool hovered = ImGui::IsItemHovered();

    ImVec2 center(cursor.x + size.x * 0.5f, cursor.y + size.y * 0.5f);
    ImVec4 iconCol = active ? Theme::COLOR_ACCENT_CYAN : (hovered ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_SECONDARY);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Draw(drawList, type, center, std::min(size.x, size.y) * 0.75f, ImGui::GetColorU32(iconCol), active);

    if (tooltip && hovered) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::PopID();

    return clicked;
}

} // namespace khepri::ui

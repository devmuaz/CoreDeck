//
// Created by AbdulMuaz Aqeel on 30/09/2026.
//

#ifndef COREDECK_GUI_UTILITIES_H
#define COREDECK_GUI_UTILITIES_H

#include <cstddef>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

namespace CoreDeck {
    struct JdkInfo;

    void CopyToBuffer(char *buffer, std::size_t size, const std::string &value);

    bool SameJdkHome(const std::string &left, const std::string &right);

    std::string JavaRuntimeLabel(const JdkInfo &jdk);

    // A drawn spinner calls this. The frame loop consumes it to keep redrawing.
    void RequestSpinnerFrame();

    bool ConsumeSpinnerFrameRequest();

    template<typename Draw>
    float MeasureContentHeight(const Draw &draw, const char *id = "##MeasureContent") {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        const ImVec2 cursorPos = window->DC.CursorPos;
        const ImVec2 cursorMax = window->DC.CursorMaxPos;
        const ImVec2 idealMax = window->DC.IdealMaxPos;
        const ImVec2 cursorPrev = window->DC.CursorPosPrevLine;
        const ImVec2 currLine = window->DC.CurrLineSize;
        const float currLineText = window->DC.CurrLineTextBaseOffset;
        const ImVec2 prevLine = window->DC.PrevLineSize;
        const float prevLineText = window->DC.PrevLineTextBaseOffset;
        const bool sameLine = window->DC.IsSameLine;
        const float indentX = window->DC.Indent.x;
        const bool setPos = window->DC.IsSetPos;

        window->DC.CursorPos.y = window->Pos.y - 100000.0F;

        ImGui::PushID(id != nullptr ? id : "##MeasureContent");
        ImGui::BeginDisabled();
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
        ImGui::BeginGroup();
        draw();
        ImGui::EndGroup();
        const float height = ImGui::GetItemRectSize().y;
        ImGui::PopStyleVar();
        ImGui::PopItemFlag();
        ImGui::EndDisabled();
        ImGui::PopID();

        window->DC.CursorPos = cursorPos;
        window->DC.CursorMaxPos = cursorMax;
        window->DC.IdealMaxPos = idealMax;
        window->DC.CursorPosPrevLine = cursorPrev;
        window->DC.CurrLineSize = currLine;
        window->DC.CurrLineTextBaseOffset = currLineText;
        window->DC.PrevLineSize = prevLine;
        window->DC.PrevLineTextBaseOffset = prevLineText;
        window->DC.IsSameLine = sameLine;
        window->DC.Indent.x = indentX;
        window->DC.IsSetPos = setPos;
        return height;
    }
}

#endif // COREDECK_GUI_UTILITIES_H

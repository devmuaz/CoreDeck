//
// Created by AbdulMuaz Aqeel on 30/09/2026.
//

#ifndef COREDECK_GUI_UTILITIES_H
#define COREDECK_GUI_UTILITIES_H

#include <cstddef>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

namespace CoreDeck::Core {
    struct JdkInfo;
}

namespace CoreDeck::Gui {

    void CopyToBuffer(char *buffer, std::size_t size, const std::string &value);

    bool SameJdkHome(const std::string &left, const std::string &right);

    std::string JavaRuntimeLabel(const Core::JdkInfo &jdk);

    // A drawn spinner calls this. The frame loop consumes it to keep redrawing.
    void RequestSpinnerFrame();

    bool ConsumeSpinnerFrameRequest();

    // Restores the parent layout cursor when a measurement pass moves it.
    class ImGuiCursorState {
    public:
        explicit ImGuiCursorState(ImGuiWindow *window)
            : m_Window(window), m_CursorPos(window->DC.CursorPos), m_CursorMaxPos(window->DC.CursorMaxPos), m_IdealMaxPos(window->DC.IdealMaxPos), m_CursorPosPrevLine(window->DC.CursorPosPrevLine), m_CurrLineSize(window->DC.CurrLineSize), m_CurrLineTextBaseOffset(window->DC.CurrLineTextBaseOffset), m_PrevLineSize(window->DC.PrevLineSize), m_PrevLineTextBaseOffset(window->DC.PrevLineTextBaseOffset), m_SameLine(window->DC.IsSameLine), m_IndentX(window->DC.Indent.x), m_SetPos(window->DC.IsSetPos) {
        }

        ~ImGuiCursorState() {
            m_Window->DC.CursorPos = m_CursorPos;
            m_Window->DC.CursorMaxPos = m_CursorMaxPos;
            m_Window->DC.IdealMaxPos = m_IdealMaxPos;
            m_Window->DC.CursorPosPrevLine = m_CursorPosPrevLine;
            m_Window->DC.CurrLineSize = m_CurrLineSize;
            m_Window->DC.CurrLineTextBaseOffset = m_CurrLineTextBaseOffset;
            m_Window->DC.PrevLineSize = m_PrevLineSize;
            m_Window->DC.PrevLineTextBaseOffset = m_PrevLineTextBaseOffset;
            m_Window->DC.IsSameLine = m_SameLine;
            m_Window->DC.Indent.x = m_IndentX;
            m_Window->DC.IsSetPos = m_SetPos;
        }

        ImGuiCursorState(const ImGuiCursorState &) = delete;
        ImGuiCursorState &operator=(const ImGuiCursorState &) = delete;
        ImGuiCursorState(ImGuiCursorState &&) = delete;
        ImGuiCursorState &operator=(ImGuiCursorState &&) = delete;

    private:
        ImGuiWindow *m_Window;
        ImVec2 m_CursorPos;
        ImVec2 m_CursorMaxPos;
        ImVec2 m_IdealMaxPos;
        ImVec2 m_CursorPosPrevLine;
        ImVec2 m_CurrLineSize;
        float m_CurrLineTextBaseOffset;
        ImVec2 m_PrevLineSize;
        float m_PrevLineTextBaseOffset;
        bool m_SameLine;
        float m_IndentX;
        bool m_SetPos;
    };

    template<typename Content>
    float MeasureContentHeight(const Content &content, const char *id = "##MeasureContent") {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        const ImGuiCursorState cursor(window);
        window->DC.CursorPos.y = window->Pos.y - 100000.0F;

        ImGui::PushID(id != nullptr ? id : "##MeasureContent");
        ImGui::BeginDisabled();
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
        ImGui::BeginGroup();
        content();
        ImGui::EndGroup();
        const float height = ImGui::GetItemRectSize().y;
        ImGui::PopStyleVar();
        ImGui::PopItemFlag();
        ImGui::EndDisabled();
        ImGui::PopID();
        return height;
    }
}

#endif // COREDECK_GUI_UTILITIES_H

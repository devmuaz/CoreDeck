//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "imgui.h"

#include "../widgets.h"

namespace CoreDeck {
    void StyleColor::Push(ImGuiCol idx, const ImVec4 &color) {
        ImGui::PushStyleColor(idx, color);
        m_Count++;
    }

    StyleColor::~StyleColor() {
        if (m_Count > 0) {
            ImGui::PopStyleColor(m_Count);
        }
    }

    void StyleVar::Push(ImGuiStyleVar idx, float val) {
        ImGui::PushStyleVar(idx, val);
        m_Count++;
    }

    void StyleVar::Push(ImGuiStyleVar idx, const ImVec2 &val) {
        ImGui::PushStyleVar(idx, val);
        m_Count++;
    }

    StyleVar::~StyleVar() {
        if (m_Count > 0) {
            ImGui::PopStyleVar(m_Count);
        }
    }

    PickerTableStyle::PickerTableStyle() {
        Colors.Push(ImGuiCol_ChildBg, HexColor(Colors::SURFACE1));
        Colors.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
        Colors.Push(ImGuiCol_TableHeaderBg, HexColor(Colors::SURFACE2));
        Colors.Push(ImGuiCol_TableRowBg, HexColor(Colors::SHADOW, 0.0F));
        Colors.Push(ImGuiCol_TableRowBgAlt, HexColor(Colors::SURFACE2, 0.28F));
        Colors.Push(ImGuiCol_TableBorderLight, HexColor(Colors::SURFACE3));
        Colors.Push(ImGuiCol_TableBorderStrong, HexColor(Colors::SURFACE4));
        Colors.Push(ImGuiCol_Header, HexColor(Colors::SURFACE3, 0.65F));
        Colors.Push(ImGuiCol_HeaderHovered, HexColor(Colors::SURFACE4, 0.85F));
        Colors.Push(ImGuiCol_HeaderActive, HexColor(Colors::BORDER_SUBTLE));

        Vars.Push(ImGuiStyleVar_ChildRounding, 6.0F);
        Vars.Push(ImGuiStyleVar_ChildBorderSize, 1.0F);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(1.0F, 1.0F));
        Vars.Push(ImGuiStyleVar_CellPadding, ImVec2(8.0F, 8.0F));
    }
}

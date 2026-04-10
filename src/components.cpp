//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "components.h"

#include "imgui.h"
#include "utilities.h"

namespace CoreDeck {
    bool PrimaryButton(const char *label, const bool isEnabled) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.push(ImGuiCol_Button, HexColor("#1A1A1C"));
        sc.push(ImGuiCol_ButtonHovered, HexColor("#2E2E30", 0.6f));
        sc.push(ImGuiCol_ButtonActive, HexColor("#0F0F12"));
        sc.push(ImGuiCol_Text, HexColor("#F2F2F2"));
        sc.push(ImGuiCol_Border, HexColor("#4D4D52"));

        if (!isEnabled) ImGui::EndDisabled();
        return ImGui::Button(label);
    }

    bool NegativeButton(const char *label, const bool isEnabled) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.push(ImGuiCol_Button, HexColor("#CC261F", 0.10f));
        sc.push(ImGuiCol_ButtonHovered, HexColor("#CC261F", 0.20f));
        sc.push(ImGuiCol_ButtonActive, HexColor("#CC261F", 0.30f));
        sc.push(ImGuiCol_Text, HexColor("#E64D40"));
        sc.push(ImGuiCol_Border, HexColor("#E64D40"));

        if (!isEnabled) ImGui::EndDisabled();
        return ImGui::Button(label);
    }

    bool WarningButton(const char *label, const bool isEnabled) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.push(ImGuiCol_Button, HexColor("#D9B31A", 0.10f));
        sc.push(ImGuiCol_ButtonHovered, HexColor("#D9B31A", 0.20f));
        sc.push(ImGuiCol_ButtonActive, HexColor("#D9B31A", 0.30f));
        sc.push(ImGuiCol_Text, HexColor("#E6BF26"));
        sc.push(ImGuiCol_Border, HexColor("#E6BF26"));

        if (!isEnabled) ImGui::EndDisabled();
        return ImGui::Button(label);
    }

    bool PositiveButton(const char *label, const bool isEnabled) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.push(ImGuiCol_Button, HexColor("#26B333", 0.10f));
        sc.push(ImGuiCol_ButtonHovered, HexColor("#26B333", 0.20f));
        sc.push(ImGuiCol_ButtonActive, HexColor("#26B333", 0.30f));
        sc.push(ImGuiCol_Text, HexColor("#33CC47"));
        sc.push(ImGuiCol_Border, HexColor("#33CC47"));

        if (!isEnabled) ImGui::EndDisabled();
        return ImGui::Button(label);
    }

    void StatusBadge(const char *label, const bool isActive) {
        StyleColor sc;
        StyleVar sv;

        if (isActive) {
            sc.push(ImGuiCol_Button, HexColor("#26B333", 0.10f));
            sc.push(ImGuiCol_Text, HexColor("#33CC47"));
        } else {
            sc.push(ImGuiCol_Button, HexColor("#CC261F", 0.10f));
            sc.push(ImGuiCol_Text, HexColor("#E64D40"));
        }
        sc.push(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_Button]);
        sc.push(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_Button]);

        sv.push(ImGuiStyleVar_FrameBorderSize, 0.0f);
        sv.push(ImGuiStyleVar_FrameRounding, 6.0f);
        sv.push(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));

        ImGui::Button(label);
    }

    bool SelectableItem(const char *label, const bool isSelected, const char *rightText, const ImVec4 &rightColor) {
        StyleColor sc;
        StyleVar sv;

        if (isSelected) sc.push(ImGuiCol_Button, HexColor("#29292B", 0.4f));
        else sc.push(ImGuiCol_Button, HexColor("#000000", 0.0f));

        sc.push(ImGuiCol_ButtonHovered, HexColor("#29292B", 0.4f));
        sc.push(ImGuiCol_ButtonActive, HexColor("#29292B", 0.8f));
        sc.push(ImGuiCol_Text, HexColor("#F2F2F2"));

        sv.push(ImGuiStyleVar_FrameRounding, 6.0f);
        sv.push(ImGuiStyleVar_FrameBorderSize, 0.0f);
        sv.push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

        const bool clicked = ImGui::Button(label, ImVec2(-1.0f, 0.0f));

        if (rightText && rightText[0] != '\0') {
            const ImVec2 textSize = ImGui::CalcTextSize(rightText);
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;

            const auto textPos = ImVec2(
                itemMax.x - textSize.x - padding.x,
                itemMin.y + (itemMax.y - itemMin.y - textSize.y) * 0.5f
            );

            ImGui::GetWindowDrawList()->AddText(
                textPos,
                ImGui::ColorConvertFloat4ToU32(rightColor),
                rightText
            );
        }

        return clicked;
    }

    void PropertyText(const char *label, const char *value) {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine();
        ImGui::Text("%s", value);
    }

    void PropertyTextWrapped(const char *label, const char *value) {
        StyleColor sc;
        ImGui::Text("%s", label);
        ImGui::SameLine();
        sc.push(ImGuiCol_Text, HexColor("#66666B"));
        ImGui::TextWrapped("%s", value);
    }

    bool CollapsingHeader(const char *label, const ImGuiTreeNodeFlags flags) {
        StyleColor sc;
        sc.push(ImGuiCol_Header, HexColor("#000000", 0.0f));
        sc.push(ImGuiCol_HeaderHovered, HexColor("#000000", 0.0f));
        sc.push(ImGuiCol_HeaderActive, HexColor("#000000", 0.0f));
        sc.push(ImGuiCol_Border, HexColor("#000000", 0.0f));
        sc.push(ImGuiCol_BorderShadow, HexColor("#000000", 0.0f));
        sc.push(ImGuiCol_Text, HexColor("#969696"));
        return ImGui::CollapsingHeader(label, flags);
    }
}

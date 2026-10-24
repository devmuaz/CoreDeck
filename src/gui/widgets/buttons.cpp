//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <numbers>

#include "imgui.h"

#include "../utilities.h"
#include "../widgets.h"
#include "internal.h"

namespace CoreDeck::Gui {
    namespace {
        ImVec2 PointOnCircle(const ImVec2 &center, const float radius, const float angle) {
            return {center.x + (ImCos(angle) * radius), center.y + (ImSin(angle) * radius)};
        }
    }

    void PaintSpinner(ImDrawList *draw, const ImVec2 &center, const float radius, const float thickness, const ImU32 color) {
        RequestSpinnerFrame();
        const auto time = static_cast<float>(ImGui::GetTime());
        constexpr float K_TURN = std::numbers::pi_v<float> * 2.0F;
        constexpr float K_SWEEP = std::numbers::pi_v<float> * 1.15F;
        const float start = time * (K_TURN * 0.8F);
        const float end = start + K_SWEEP;

        ImVec4 trackColor = ImGui::ColorConvertU32ToFloat4(color);
        trackColor.w *= 0.28F;
        constexpr int K_TRACK_SEGMENTS = 32;
        const float trackEnd = K_TURN * (static_cast<float>(K_TRACK_SEGMENTS - 1) / static_cast<float>(K_TRACK_SEGMENTS));
        draw->PathArcTo(center, radius, 0.0F, trackEnd, K_TRACK_SEGMENTS - 1);
        draw->PathStroke(ImGui::GetColorU32(trackColor), thickness, ImDrawFlags_Closed);

        draw->PathArcTo(center, radius, start, end, 24);
        draw->PathStroke(color, thickness);
        const float cap = thickness * 0.5F;
        draw->AddCircleFilled(PointOnCircle(center, radius, start), cap, color);
        draw->AddCircleFilled(PointOnCircle(center, radius, end), cap, color);
    }

    namespace {
        bool StyledButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
            const char *text = label != nullptr ? label : "";
            const bool dimmed = !isEnabled && !hasSpinner;
            if (dimmed) {
                ImGui::BeginDisabled();
            }

            bool clicked = false;
            if (!hasSpinner) {
                clicked = ImGui::Button(text, size);
            } else {
                const ImGuiStyle &style = ImGui::GetStyle();
                const float diameter = ImGui::GetTextLineHeight();
                const float gap = style.ItemInnerSpacing.x;
                const char *textEnd = ImGui::FindRenderedTextEnd(text);
                const ImVec2 textSize = ImGui::CalcTextSize(text, textEnd);
                const float contentWidth = diameter + gap + textSize.x;
                ImVec2 buttonSize = size;
                if (buttonSize.x <= 0.0F) {
                    buttonSize.x = contentWidth + (style.FramePadding.x * 2.0F);
                }

                ImGui::PushID(text);
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                ImGui::Button("##busy", buttonSize);
                ImGui::PopItemFlag();
                ImGui::PopItemFlag();
                ImGui::PopID();

                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                const float innerWidth = std::max(0.0F, (max.x - min.x) - (style.FramePadding.x * 2.0F));
                const float originX = contentWidth < innerWidth
                                          ? min.x + (((max.x - min.x) - contentWidth) * 0.5F)
                                          : min.x + style.FramePadding.x;
                const float midY = (min.y + max.y) * 0.5F;
                const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
                const float thickness = std::max(2.0F * GetDpiScale(), diameter * 0.14F);
                const float radius = std::max(1.0F, (diameter - thickness) * 0.5F);

                ImDrawList *draw = ImGui::GetWindowDrawList();
                draw->PushClipRect(min, max, true);
                PaintSpinner(draw, ImVec2(originX + (diameter * 0.5F), midY), radius, thickness, ink);
                draw->AddText(ImVec2(originX + diameter + gap, midY - (textSize.y * 0.5F)), ink, text, textEnd);
                draw->PopClipRect();
            }

            if (dimmed) {
                ImGui::EndDisabled();
            }
            return clicked;
        }
    }

    bool PrimaryButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
        if (IsLightColorScheme()) {
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE4));
        } else {
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE4, 0.6F));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE0));
        }
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        sc.Push(ImGuiCol_Border, HexColor(Colors::BORDER_STRONG));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool NegativeButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::NEGATIVE_STRONG, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::NEGATIVE_STRONG, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::NEGATIVE_STRONG, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::NEGATIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::NEGATIVE));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool WarningButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::WARNING, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::WARNING, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::WARNING, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::WARNING_STRONG));
        sc.Push(ImGuiCol_Border, HexColor(Colors::WARNING_STRONG));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool PositiveButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::POSITIVE_FILL, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::POSITIVE_FILL, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::POSITIVE));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool PickerButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleVar sv;
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0F, 0.5F));
        return PrimaryButton(label, isEnabled, size, hasSpinner);
    }

    bool ToggleButton(const char *label, bool &isToggled, const ImVec2 size) {
        StyleColor sc;
        if (isToggled) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::TEXT_PRIMARY, 0.10F));
            sc.Push(ImGuiCol_Border, HexColor(Colors::TEXT_PRIMARY, 0.75F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        }
        const bool clicked = ImGui::Button(label, size);
        if (clicked) {
            isToggled = !isToggled;
        }
        return clicked;
    }

    void StatusBadge(const char *label, const bool isActive) {
        StyleColor sc;
        StyleVar sv;

        if (isActive) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.10F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::NEGATIVE_STRONG, 0.10F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::NEGATIVE));
        }
        sc.Push(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_Button]);
        sc.Push(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_Button]);

        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
        sv.Push(ImGuiStyleVar_FrameRounding, 6.0F);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(6.0F, 2.0F));

        ImGui::Button(label);
    }

    void Spinner(const ImVec4 &color, const float diameter) {
        const float dpi = GetDpiScale();
        const float size = diameter > 0.0F ? diameter : ImGui::GetTextLineHeight();
        const float thickness = std::max(2.0F * dpi, size * 0.12F);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        const float radius = std::max(1.0F, (size - thickness) * 0.5F);
        const ImVec2 center(origin.x + (size * 0.5F), origin.y + (size * 0.5F));
        PaintSpinner(ImGui::GetWindowDrawList(), center, radius, thickness, ImGui::GetColorU32(color));
    }
}

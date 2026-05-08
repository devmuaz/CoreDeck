//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "imgui.h"

#include "widgets.h"
#include "theme.h"

namespace CoreDeck {
    PickerTableStyle::PickerTableStyle() {
        Colors.Push(ImGuiCol_ChildBg, HexColor(Colors::Surface1));
        Colors.Push(ImGuiCol_Border, HexColor(Colors::Surface4));
        Colors.Push(ImGuiCol_TableHeaderBg, HexColor(Colors::Surface2));
        Colors.Push(ImGuiCol_TableRowBg, HexColor(Colors::Shadow, 0.0f));
        Colors.Push(ImGuiCol_TableRowBgAlt, HexColor(Colors::Surface2, 0.28f));
        Colors.Push(ImGuiCol_TableBorderLight, HexColor(Colors::Surface3));
        Colors.Push(ImGuiCol_TableBorderStrong, HexColor(Colors::Surface4));
        Colors.Push(ImGuiCol_Header, HexColor(Colors::Surface3, 0.65f));
        Colors.Push(ImGuiCol_HeaderHovered, HexColor(Colors::Surface4, 0.85f));
        Colors.Push(ImGuiCol_HeaderActive, HexColor(Colors::BorderSubtle));

        Vars.Push(ImGuiStyleVar_ChildRounding, 6.0f);
        Vars.Push(ImGuiStyleVar_ChildBorderSize, 1.0f);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
        Vars.Push(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
    }

    bool PrimaryButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::Surface2));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::Surface4, 0.6f));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::Surface0));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TextPrimary));
        sc.Push(ImGuiCol_Border, HexColor(Colors::BorderStrong));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) ImGui::EndDisabled();
        return clicked;
    }

    bool NegativeButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::NegativeStrong, 0.10f));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::NegativeStrong, 0.20f));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::NegativeStrong, 0.30f));
        sc.Push(ImGuiCol_Text, HexColor(Colors::Negative));
        sc.Push(ImGuiCol_Border, HexColor(Colors::Negative));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) ImGui::EndDisabled();
        return clicked;
    }

    bool WarningButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::Warning, 0.10f));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::Warning, 0.20f));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::Warning, 0.30f));
        sc.Push(ImGuiCol_Text, HexColor(Colors::WarningStrong));
        sc.Push(ImGuiCol_Border, HexColor(Colors::WarningStrong));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) ImGui::EndDisabled();
        return clicked;
    }

    bool PositiveButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) ImGui::BeginDisabled();

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::PositiveFill, 0.10f));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::PositiveFill, 0.20f));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::PositiveFill, 0.30f));
        sc.Push(ImGuiCol_Text, HexColor(Colors::Positive));
        sc.Push(ImGuiCol_Border, HexColor(Colors::Positive));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) ImGui::EndDisabled();
        return clicked;
    }

    bool PickerButton(const char *label, const bool isEnabled, const ImVec2 size) {
        StyleVar sv;
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        return PrimaryButton(label, isEnabled, size);
    }

    bool ToggleButton(const char *label, bool &isToggled, const ImVec2 size) {
        StyleColor sc;
        if (isToggled) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::White, 0.10f));
            sc.Push(ImGuiCol_Border, HexColor(Colors::White, 0.75f));
            sc.Push(ImGuiCol_Text, HexColor(Colors::White));
        }
        const bool clicked = ImGui::Button(label, size);
        if (clicked) isToggled = !isToggled;
        return clicked;
    }

    void StatusBadge(const char *label, const bool isActive) {
        StyleColor sc;
        StyleVar sv;

        if (isActive) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::PositiveFill, 0.10f));
            sc.Push(ImGuiCol_Text, HexColor(Colors::Positive));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::NegativeStrong, 0.10f));
            sc.Push(ImGuiCol_Text, HexColor(Colors::Negative));
        }
        sc.Push(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_Button]);
        sc.Push(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_Button]);

        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0f);
        sv.Push(ImGuiStyleVar_FrameRounding, 6.0f);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));

        ImGui::Button(label);
    }

    bool SelectableItem(
        const char *label,
        const bool isSelected,
        const char *rightText,
        const ImVec4 &rightColor,
        const char *leftIcon,
        const ImVec4 &leftIconColor
    ) {
        StyleColor sc;
        StyleVar sv;

        if (isSelected) sc.Push(ImGuiCol_Button, HexColor(Colors::Surface3, 0.4f));
        else sc.Push(ImGuiCol_Button, HexColor(Colors::Shadow, 0.0f));

        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::Surface3, 0.4f));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::Surface3, 0.8f));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TextPrimary));

        sv.Push(ImGuiStyleVar_FrameRounding, 6.0f);
        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0f);
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

        std::string buttonLabel;
        if (leftIcon && leftIcon[0] != '\0') {
            buttonLabel = leftIcon;
            buttonLabel += "  ";
            buttonLabel += label;
        } else {
            buttonLabel = label;
        }

        const bool clicked = ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0f, 0.0f));

        if (leftIcon && leftIcon[0] != '\0') {
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            const ImVec2 iconSize = ImGui::CalcTextSize(leftIcon);

            const auto iconPos = ImVec2(
                itemMin.x + padding.x,
                itemMin.y + (itemMax.y - itemMin.y - iconSize.y) * 0.5f
            );

            ImGui::GetWindowDrawList()->AddText(
                iconPos,
                ImGui::ColorConvertFloat4ToU32(leftIconColor),
                leftIcon
            );
        }

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

    bool PropertyText(const char *label, const char *value, const bool isClickable, const bool hasSpaceBetween) {
        ImGui::TextDisabled("%s", label);

        if (hasSpaceBetween) {
            const float valueWidth = ImGui::CalcTextSize(value).x;
            ImGui::SameLine(
                ImGui::GetContentRegionAvail().x - valueWidth + ImGui::GetCursorPosX() - ImGui::GetCursorStartPos().x
            );
        } else {
            ImGui::SameLine();
        }

        if (!isClickable) {
            ImGui::Text("%s", value);
            return false;
        }

        ImGui::PushID(label);
        const ImVec2 textPos = ImGui::GetCursorScreenPos();
        const ImVec2 textSize = ImGui::CalcTextSize(value);

        const bool clicked = ImGui::InvisibleButton("##link", textSize);
        const bool hovered = ImGui::IsItemHovered();

        const ImU32 color = hovered
                                ? ImGui::ColorConvertFloat4ToU32(HexColor(Colors::AccentInfoSoft))
                                : ImGui::ColorConvertFloat4ToU32(HexColor(Colors::AccentInfo));

        ImGui::GetWindowDrawList()->AddText(textPos, color, value);

        if (hovered) {
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(textPos.x, textPos.y + textSize.y),
                ImVec2(textPos.x + textSize.x, textPos.y + textSize.y),
                color
            );
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::PopID();
        return clicked;
    }

    void PropertyTextWrapped(const char *label, const char *value, const bool invertColors) {
        StyleColor sc;
        if (invertColors) sc.Push(ImGuiCol_Text, HexColor(Colors::TextMuted));
        ImGui::Text("%s", label);
        ImGui::SameLine();

        if (invertColors) {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TextPrimary));
        } else {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TextMuted));
        }
        ImGui::TextWrapped("%s", value);
    }

    bool CategoryChip(const char *label, const bool isSelected) {
        StyleColor sc;
        StyleVar sv;

        if (isSelected) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::PositiveFill, 0.16f));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::PositiveFill, 0.24f));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::PositiveFill, 0.32f));
            sc.Push(ImGuiCol_Text, HexColor(Colors::Positive));
            sc.Push(ImGuiCol_Border, HexColor(Colors::Positive));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::Surface2));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::Surface3));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::Surface4));
            sc.Push(ImGuiCol_Text, HexColor(Colors::TextHint));
            sc.Push(ImGuiCol_Border, HexColor(Colors::Surface4));
        }

        sv.Push(ImGuiStyleVar_FrameRounding, 999.0f);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
        sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0f);

        return ImGui::Button(label);
    }

    bool CollapsingHeader(const char *label, const ImGuiTreeNodeFlags flags) {
        StyleColor sc;
        sc.Push(ImGuiCol_Header, HexColor(Colors::Shadow, 0.0f));
        sc.Push(ImGuiCol_HeaderHovered, HexColor(Colors::Shadow, 0.0f));
        sc.Push(ImGuiCol_HeaderActive, HexColor(Colors::Shadow, 0.0f));
        sc.Push(ImGuiCol_Border, HexColor(Colors::Shadow, 0.0f));
        sc.Push(ImGuiCol_BorderShadow, HexColor(Colors::Shadow, 0.0f));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TextOnBright));
        return ImGui::CollapsingHeader(label, flags);
    }

    DialogResult SimpleDialog(const DialogData &data) {
        auto result = DialogResult::None;
        const std::string title = StrConcat(data.Title, "###", data.Id);

        if (data.IsOpen && !ImGui::IsPopupOpen(title.c_str())) {
            ImGui::OpenPopup(title.c_str());
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal(title.c_str(), data.IsBusy ? nullptr : &data.IsOpen, Flags)) {
            if (!data.IsOpen) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return result;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextMuted));
            ImGui::TextWrapped("%s", data.Message);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            if (data.IsBusy) {
                ImGui::BeginDisabled();
                const char *busyLabel = data.BusyButtonTitle ? data.BusyButtonTitle : data.ConfirmButtonTitle;
                switch (data.Type) {
                    case DialogType::Negative:
                        NegativeButton(busyLabel, false, ImVec2(halfWidth, 0));
                        break;
                    case DialogType::Positive:
                        PositiveButton(busyLabel, false, ImVec2(halfWidth, 0));
                        break;
                    default:
                        PrimaryButton(busyLabel, false, ImVec2(halfWidth, 0));
                        break;
                }
                ImGui::SameLine();
                PrimaryButton(data.CancelButtonTitle, false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                bool confirmed = false;
                switch (data.Type) {
                    case DialogType::Negative:
                        confirmed = NegativeButton(data.ConfirmButtonTitle, true, ImVec2(halfWidth, 0));
                        break;
                    case DialogType::Positive:
                        confirmed = PositiveButton(data.ConfirmButtonTitle, true, ImVec2(halfWidth, 0));
                        break;
                    default:
                        confirmed = PrimaryButton(data.ConfirmButtonTitle, true, ImVec2(halfWidth, 0));
                        break;
                }
                if (confirmed) {
                    result = DialogResult::Confirmed;
                }

                ImGui::SameLine();
                if (PrimaryButton(data.CancelButtonTitle, true, ImVec2(halfWidth, 0))) {
                    result = DialogResult::Cancelled;
                    data.IsOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        return result;
    }
}

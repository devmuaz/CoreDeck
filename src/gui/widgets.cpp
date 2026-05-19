//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "imgui.h"

#include "widgets.h"
#include "theme.h"

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

    bool PrimaryButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) {
            ImGui::BeginDisabled();
        }

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE4, 0.6F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE0));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        sc.Push(ImGuiCol_Border, HexColor(Colors::BORDER_STRONG));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) {
            ImGui::EndDisabled();
        }
        return clicked;
    }

    bool NegativeButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) {
            ImGui::BeginDisabled();
        }

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::NEGATIVE_STRONG, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::NEGATIVE_STRONG, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::NEGATIVE_STRONG, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::NEGATIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::NEGATIVE));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) {
            ImGui::EndDisabled();
        }
        return clicked;
    }

    bool WarningButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) {
            ImGui::BeginDisabled();
        }

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::WARNING, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::WARNING, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::WARNING, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::WARNING_STRONG));
        sc.Push(ImGuiCol_Border, HexColor(Colors::WARNING_STRONG));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) {
            ImGui::EndDisabled();
        }
        return clicked;
    }

    bool PositiveButton(const char *label, const bool isEnabled, const ImVec2 size) {
        if (!isEnabled) {
            ImGui::BeginDisabled();
        }

        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::POSITIVE_FILL, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::POSITIVE_FILL, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::POSITIVE));

        const bool clicked = ImGui::Button(label, size);
        if (!isEnabled) {
            ImGui::EndDisabled();
        }
        return clicked;
    }

    bool PickerButton(const char *label, const bool isEnabled, const ImVec2 size) {
        StyleVar sv;
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0F, 0.5F));
        return PrimaryButton(label, isEnabled, size);
    }

    bool ToggleButton(const char *label, bool &isToggled, const ImVec2 size) {
        StyleColor sc;
        if (isToggled) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::WHITE, 0.10F));
            sc.Push(ImGuiCol_Border, HexColor(Colors::WHITE, 0.75F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::WHITE));
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

        if (isSelected) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE3, 0.4F));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::SHADOW, 0.0F));
        }

        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3, 0.4F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE3, 0.8F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));

        sv.Push(ImGuiStyleVar_FrameRounding, 6.0F);
        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0F, 0.5F));

        std::string buttonLabel;
        if (leftIcon && leftIcon[0] != '\0') {
            buttonLabel = leftIcon;
            buttonLabel += "  ";
            buttonLabel += label;
        } else {
            buttonLabel = label;
        }

        const bool clicked = ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0F, 0.0F));

        if (leftIcon && leftIcon[0] != '\0') {
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            const ImVec2 iconSize = ImGui::CalcTextSize(leftIcon);

            const auto iconPos = ImVec2(
                itemMin.x + padding.x,
                itemMin.y + ((itemMax.y - itemMin.y - iconSize.y) * 0.5F)
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
                itemMin.y + ((itemMax.y - itemMin.y - textSize.y) * 0.5F)
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
                                ? ImGui::ColorConvertFloat4ToU32(HexColor(Colors::ACCENT_INFO_SOFT))
                                : ImGui::ColorConvertFloat4ToU32(HexColor(Colors::ACCENT_INFO));

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
        if (invertColors) {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
        }
        ImGui::Text("%s", label);
        ImGui::SameLine();

        if (invertColors) {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        } else {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
        }
        ImGui::TextWrapped("%s", value);
    }

    bool CategoryChip(const char *label, const bool isSelected) {
        StyleColor sc;
        StyleVar sv;

        if (isSelected) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.16F));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::POSITIVE_FILL, 0.24F));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::POSITIVE_FILL, 0.32F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
            sc.Push(ImGuiCol_Border, HexColor(Colors::POSITIVE));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE4));
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_HINT));
            sc.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
        }

        sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0F, 5.0F));
        sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);

        return ImGui::Button(label);
    }

    bool CollapsingHeader(const char *label, const ImGuiTreeNodeFlags flags) {
        StyleColor sc;
        sc.Push(ImGuiCol_Header, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_HeaderHovered, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_HeaderActive, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_Border, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_BorderShadow, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_ON_BRIGHT));
        return ImGui::CollapsingHeader(label, flags);
    }

    DialogResult SimpleDialog(const DialogData &data) {
        auto result = DialogResult::None;
        const std::string title = StrConcat(data.Title, "###", data.Id);

        if (data.IsOpen && !ImGui::IsPopupOpen(title.c_str())) {
            ImGui::OpenPopup(title.c_str());
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImVec2(Em(42.0F), 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags FLAGS =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking;

        if (RoundedBeginPopupModal(title.c_str(), data.IsBusy ? nullptr : &data.IsOpen, FLAGS)) {
            if (!data.IsOpen) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return result;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::TextWrapped("%s", data.Message);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = ((ImGui::GetContentRegionAvail().x - spacing) * 0.5F);

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

    MenuStyle::MenuStyle() {
        const float s = GetDpiScale();
        Vars.Push(ImGuiStyleVar_PopupRounding, 6.0F * s);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F * s, 12.0F * s));
        Vars.Push(ImGuiStyleVar_FramePadding, ImVec2(12.0F * s, 10.0F * s));
        Vars.Push(ImGuiStyleVar_ItemSpacing, ImVec2(16.0F * s, 12.0F * s));
    }

    namespace {
        bool RoundedMenuItemImpl(const char *label, const char *shortcut, bool isSelected, bool *pIsSelected, bool isEnabled) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->ChannelsSplit(2);
            drawList->ChannelsSetCurrent(1);

            // Suppress ImGui's built-in (square) hover/active fill; we draw our own.
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

            const bool pressed =
                pIsSelected
                    ? ImGui::MenuItem(label, shortcut, pIsSelected, isEnabled)
                    : ImGui::MenuItem(label, shortcut, isSelected, isEnabled);

            ImGui::PopStyleColor(2);

            drawList->ChannelsSetCurrent(0);
            if (isEnabled && ImGui::IsItemHovered()) {
                const ImVec2 pMin = ImGui::GetItemRectMin();
                const ImVec2 pMax = ImGui::GetItemRectMax();
                const ImVec4 fill = ImGui::IsItemActive() ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
                drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 4.0F * GetDpiScale());
            }
            drawList->ChannelsMerge();
            return pressed;
        }
    }

    bool RoundedMenuItem(const char *label, const char *shortcut, const bool isSelected, const bool isEnabled) {
        return RoundedMenuItemImpl(label, shortcut, isSelected, nullptr, isEnabled);
    }

    bool RoundedMenuItem(const char *label, const char *shortcut, bool *pIsSelected, const bool isEnabled) {
        return RoundedMenuItemImpl(label, shortcut, false, pIsSelected, isEnabled);
    }

    bool RoundedBeginMenu(const char *label, const bool isEnabled) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

        const bool open = ImGui::BeginMenu(label, isEnabled);

        ImGui::PopStyleColor(3);

        drawList->ChannelsSetCurrent(0);
        if (isEnabled && (open || ImGui::IsItemHovered())) {
            const ImVec2 pMin = ImGui::GetItemRectMin();
            const ImVec2 pMax = ImGui::GetItemRectMax();
            const ImVec4 fill = open ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
            drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 50.0F * GetDpiScale());
        }
        drawList->ChannelsMerge();
        return open;
    }

    ComboStyle::ComboStyle() {
        const float s = GetDpiScale();
        Vars.Push(ImGuiStyleVar_PopupRounding, 6.0F * s);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F * s, 12.0F * s));
        Vars.Push(ImGuiStyleVar_FramePadding, ImVec2(14.0F * s, 8.0F * s));
        Vars.Push(ImGuiStyleVar_ItemSpacing, ImVec2(16.0F * s, 12.0F * s));
    }

    bool RoundedSelectable(const char *label, const bool isSelected, const ImGuiSelectableFlags flags, const ImVec2 &size) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

        const bool pressed = ImGui::Selectable(label, isSelected, flags, size);

        ImGui::PopStyleColor(3);

        drawList->ChannelsSetCurrent(0);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered || isSelected) {
            const ImVec2 pMin = ImGui::GetItemRectMin();
            const ImVec2 pMax = ImGui::GetItemRectMax();
            const ImVec4 fill = ImGui::IsItemActive() ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
            drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 4.0F * GetDpiScale());
        }
        drawList->ChannelsMerge();
        return pressed;
    }

    bool RoundedBeginPopupModal(const char *name, bool *pOpen, const ImGuiWindowFlags flags) {
        const bool open = ImGui::BeginPopupModal(name, nullptr, flags);
        if (!open || pOpen == nullptr) {
            return open;
        }

        const ImGuiStyle &style = ImGui::GetStyle();
        const float s = GetDpiScale();
        const float fontSize = ImGui::GetFontSize();
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();

        const ImVec2 btnMin(
            winPos.x + winSize.x - style.FramePadding.x - fontSize,
            winPos.y + style.FramePadding.y
        );
        const ImVec2 btnMax(btnMin.x + fontSize, btnMin.y + fontSize);
        const ImVec2 bbCenter((btnMin.x + btnMax.x) * 0.5F, (btnMin.y + btnMax.y) * 0.5F);
        const ImVec2 crossCenter(bbCenter.x - 0.5F, bbCenter.y - 0.5F);

        const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool hovered = windowHovered && ImGui::IsMouseHoveringRect(btnMin, btnMax, false);
        const bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), false);

        if (hovered) {
            const float baseRadius = (fontSize * 0.5F) + (4.0F * s);
            const float radius = held ? baseRadius - (1.5F * s) : baseRadius;
            const ImVec4 fill = held
                ? HexColor(Colors::NEGATIVE_STRONG)
                : HexColor(Colors::NEGATIVE, 0.85F);
            drawList->AddCircleFilled(crossCenter, radius, ImGui::GetColorU32(fill));
        }

        const float crossExtent = (fontSize * 0.5F * 0.7071F) - 1.0F;
        const ImU32 crossCol = ImGui::GetColorU32(ImGuiCol_Text);
        const float crossThick = 1.0F * s;
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y + crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y - crossExtent),
            crossCol, crossThick
        );
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y - crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y + crossExtent),
            crossCol, crossThick
        );
        drawList->PopClipRect();

        if (clicked) {
            *pOpen = false;
            ImGui::CloseCurrentPopup();
        }
        return open;
    }
}

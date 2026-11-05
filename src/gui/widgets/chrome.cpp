//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>

#include "imgui.h"

#include "widgets.h"

namespace CoreDeck::Gui {
    namespace {
        bool CategoryChip(const char *label, const char *fill, const char *accent) {
            StyleColor sc;
            StyleVar sv;
            sc.Push(ImGuiCol_Button, HexColor(fill, 0.16F));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(fill, 0.24F));
            sc.Push(ImGuiCol_ButtonActive, HexColor(fill, 0.32F));
            sc.Push(ImGuiCol_Text, HexColor(accent));
            sc.Push(ImGuiCol_Border, HexColor(accent));
            sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
            sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0F, 5.0F));
            sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);
            return ImGui::Button(label);
        }

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

    bool CategoryChip(const char *label, const bool isSelected) {
        if (isSelected) {
            return CategoryChip(label, Colors::POSITIVE_FILL, Colors::POSITIVE);
        }
        StyleColor sc;
        StyleVar sv;
        sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE4));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_HINT));
        sc.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
        sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0F, 5.0F));
        sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);
        return ImGui::Button(label);
    }

    bool CategoryChip(const char *label, const char *accent) {
        return CategoryChip(label, accent, accent);
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

    DialogResult SimpleDialog(const DialogData &data, const bool hasSpinner) {
        auto result = DialogResult::None;
        const std::string title = Platform::StrConcat(data.Title, "###", data.Id);

        constexpr ImGuiWindowFlags FLAGS =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings;

        if (!data.IsOpen && !ImGui::IsPopupOpen(title.c_str())) {
            return result;
        }

        if (BeginCenteredModal(title.c_str(), data.IsBusy ? nullptr : &data.IsOpen, ImVec2(Em(42.0F), 0), FLAGS)) {
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

            const float halfWidth = EqualButtonWidth(2);
            const bool busy = data.IsBusy;
            const bool showSpinner = busy && hasSpinner;
            const char *confirmLabel = busy && data.BusyButtonTitle != nullptr ? data.BusyButtonTitle : data.ConfirmButtonTitle;
            const ImVec2 buttonSize(halfWidth, 0);

            bool confirmed = false;
            switch (data.Type) {
                case DialogType::Negative:
                    confirmed = NegativeButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
                case DialogType::Positive:
                    confirmed = PositiveButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
                default:
                    confirmed = PrimaryButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
            }
            if (confirmed) {
                result = DialogResult::Confirmed;
                if (data.CloseOnConfirm) {
                    data.IsOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (PrimaryButton(data.CancelButtonTitle, !busy, buttonSize)) {
                result = DialogResult::Cancelled;
                data.IsOpen = false;
                ImGui::CloseCurrentPopup();
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

    void ClippedText(ImDrawList *draw, const ImVec2 &pos, const float width, const ImU32 color, const char *text) {
        if (draw == nullptr || text == nullptr || width <= 0.0F) {
            return;
        }
        if (ImGui::CalcTextSize(text).x <= width) {
            draw->AddText(pos, color, text);
            return;
        }
        const char *ellipsis = "...";
        const float keep = std::max(0.0F, width - ImGui::CalcTextSize(ellipsis).x);
        const char *fit = text;
        const char *cursor = text;
        while (*cursor != '\0') {
            const char *next = cursor + 1;
            while ((static_cast<unsigned char>(*next) & 0xC0U) == 0x80U) {
                ++next;
            }
            if (ImGui::CalcTextSize(text, next).x > keep) {
                break;
            }
            fit = next;
            cursor = next;
        }
        draw->AddText(pos, color, text, fit);
        const float used = ImGui::CalcTextSize(text, fit).x;
        draw->AddText(ImVec2(pos.x + used, pos.y), color, ellipsis);
    }

    DashboardRowResult DashboardRow(const char *id, const float width, const float contentHeight, const DashboardRowStyle &style) {
        const ImVec2 framePad = ImGui::GetStyle().FramePadding;
        const float padX = framePad.x + style.ExtraPaddingX;
        const float padY = framePad.y + style.ExtraPaddingY;
        const float height = std::max(1.0F, contentHeight + (padY * 2.0F));
        const float rowW = std::max(1.0F, width);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float hitW = std::max(1.0F, rowW - std::max(0.0F, style.HitInsetRight));
        const bool clicked = ImGui::InvisibleButton(id != nullptr ? id : "##row", ImVec2(hitW, height));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();

        const ImVec2 max(origin.x + rowW, origin.y + height);
        ImVec4 fill = HexColor(Colors::SURFACE2);
        bool paint = style.Filled;
        if (held) {
            fill = HexColor(Colors::SURFACE3, style.Filled ? 1.0F : 0.8F);
            paint = true;
        } else if (hovered || style.Selected) {
            fill = style.Filled ? HexColor(Colors::SURFACE3) : HexColor(Colors::SURFACE3, 0.4F);
            paint = true;
        }
        if (paint) {
            ImGui::GetWindowDrawList()->AddRectFilled(origin, max, ImGui::GetColorU32(fill), 6.0F);
        }
        if (style.Filled) {
            const ImVec4 border = HexColor(hovered ? Colors::BORDER_HOVER : Colors::BORDER_SUBTLE);
            ImGui::GetWindowDrawList()->AddRect(origin, max, ImGui::GetColorU32(border), 6.0F, 0, 1.0F);
        }

        return {
            .Clicked = clicked,
            .Hovered = hovered,
            .Held = held,
            .Min = origin,
            .Max = max,
            .ContentMin = ImVec2(origin.x + padX, origin.y + padY),
            .ContentMax = ImVec2(max.x - padX, max.y - padY),
        };
    }

    float IconCircleButtonDiameter() {
        return ImGui::GetTextLineHeight() + (ImGui::GetStyle().FramePadding.y * 1.0F);
    }

    bool IconCircleButton(
        const char *id,
        const char *icon,
        const bool enabled,
        const IconCircleButtonStyle &style,
        const char *tooltip
    ) {
        const float diameter = IconCircleButtonDiameter();
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(diameter, diameter));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = hovered && enabled;
        const ImVec2 boundsMin = ImGui::GetItemRectMin();
        const ImVec2 boundsMax = ImGui::GetItemRectMax();
        const ImVec2 center((boundsMin.x + boundsMax.x) * 0.5F, (boundsMin.y + boundsMax.y) * 0.5F);

        ImVec4 fill = HexColor(active ? style.HoverFill : style.Fill);
        ImVec4 glyph = HexColor(active ? style.HoverGlyph : style.Glyph);
        if (!enabled) {
            fill.w *= style.DisabledAlpha;
            glyph.w *= style.DisabledAlpha;
        }

        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddCircleFilled(center, diameter * 0.5F, ImGui::GetColorU32(fill));
        const char *glyphText = icon != nullptr ? icon : "";
        const ImVec2 iconSize = ImGui::CalcTextSize(glyphText);
        draw->AddText(
            ImVec2(center.x - (iconSize.x * 0.5F), center.y - (iconSize.y * 0.5F)),
            ImGui::GetColorU32(glyph),
            glyphText
        );
        if (tooltip != nullptr && tooltip[0] != '\0' && hovered) {
            ImGui::SetTooltip("%s", tooltip);
        }
        return enabled && pressed;
    }

    float RecentFileItemHeight() {
        return ImGui::GetTextLineHeight() + (ImGui::GetStyle().FramePadding.y * 2.0F);
    }

    float RecentFileItemWidth(const char *label) {
        const float scale = GetDpiScale();
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const float gap = 8.0F * scale;
        const float textW = ImGui::CalcTextSize(label == nullptr ? "" : label).x;
        return padding.x + textW + gap + IconCircleButtonDiameter() + padding.x;
    }

    RecentFileAction RecentFileItem(const char *id, const char *label) {
        const float scale = GetDpiScale();
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const float diameter = IconCircleButtonDiameter();
        const ImVec2 size(RecentFileItemWidth(label), RecentFileItemHeight());

        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float rowHitW = std::max(1.0F, size.x - padding.x - diameter);
        const bool rowPressed = ImGui::InvisibleButton("##recent-file", ImVec2(rowHitW, size.y));
        const bool rowHovered = ImGui::IsItemHovered();
        const bool rowHeld = ImGui::IsItemActive();

        ImDrawList *draw = ImGui::GetWindowDrawList();
        if (rowHovered || rowHeld) {
            const ImVec4 fill = rowHeld ? HexColor(Colors::SURFACE3, 0.8F) : HexColor(Colors::SURFACE3, 0.4F);
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(fill), 6.0F * scale);
        }
        draw->AddText(
            ImVec2(origin.x + padding.x, origin.y + padding.y),
            ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)),
            label
        );

        const ImVec2 circleMin(origin.x + size.x - padding.x - diameter, origin.y + ((size.y - diameter) * 0.5F));
        ImGui::SetCursorScreenPos(circleMin);
        const bool removePressed = IconCircleButton(
            "##recent-remove",
            Icons::TIMES,
            true,
            {
                .HoverFill = Colors::NEGATIVE,
                .HoverGlyph = Colors::WHITE,
            }
        );
        const bool removeHovered = ImGui::IsItemHovered();

        ImGui::PopID();
        if (removePressed) {
            return RecentFileAction::Removed;
        }
        if (rowPressed && !removeHovered) {
            return RecentFileAction::Activated;
        }
        return RecentFileAction::None;
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
        const ImU32 crossCol = hovered
                                   ? ImGui::GetColorU32(HexColor(Colors::WHITE))
                                   : ImGui::GetColorU32(ImGuiCol_Text);
        const float crossThick = 1.0F * s;
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y + crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y - crossExtent),
            crossCol,
            crossThick
        );
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y - crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y + crossExtent),
            crossCol,
            crossThick
        );
        drawList->PopClipRect();

        if (clicked) {
            *pOpen = false;
            ImGui::CloseCurrentPopup();
        }
        return open;
    }

    bool BeginCenteredModal(const char *name, bool *pOpen, const ImVec2 &size, const ImGuiWindowFlags flags) {
        if (!ImGui::IsPopupOpen(name)) {
            ImGui::OpenPopup(name);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
        return RoundedBeginPopupModal(name, pOpen, flags);
    }

    float EqualButtonWidth(const int count) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float gaps = spacing * static_cast<float>(count - 1);
        return (ImGui::GetContentRegionAvail().x - gaps) / static_cast<float>(count);
    }

    bool CategoryChipRow(const char *const *labels, const int count, int &selectedIndex) {
        bool clicked = false;
        for (int i = 0; i < count; ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (CategoryChip(labels[i], selectedIndex == i)) {
                selectedIndex = i;
                clicked = true;
            }
        }
        return clicked;
    }

    bool BeginPickerTable(const char *childId, const char *tableId, const int columns, const float height) {
        ImGui::BeginChild(childId, ImVec2(-1.0F, height), 1, ImGuiWindowFlags_NoScrollbar);
        const bool open = ImGui::BeginTable(tableId, columns, PICKER_TABLE_FLAGS, ImVec2(-1.0F, -1.0F));
        if (open) {
            ImGui::TableSetupScrollFreeze(0, 1);
        }
        return open;
    }

    void EndPickerTable(const bool open) {
        if (open) {
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
}

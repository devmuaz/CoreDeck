//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <filesystem>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#include "../../core/file_dialog.h"
#include "../../core/i18n.h"
#include "../utilities.h"
#include "../widgets.h"

namespace CoreDeck::Gui {
    void HoverTooltip(const char *text) {
        if (text == nullptr || text[0] == '\0' || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            return;
        }
        ImGui::SetTooltip("%s", text);
    }

    std::string DirectoryOf(const std::string &path) {
        if (path.empty()) {
            return {};
        }
        return std::filesystem::path(path).parent_path().string();
    }

    std::string FileNameOf(const std::string &path) {
        if (path.empty()) {
            return {};
        }
        const std::string name = std::filesystem::path(path).filename().string();
        return name.empty() ? path : name;
    }

    SecretEdit SecretField(
        const char *id,
        char *buffer,
        const std::size_t size,
        bool &revealed,
        const char *hint,
        const float rowWidth,
        const char *tooltip
    ) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttonSide = ImGui::GetFrameHeight();
        const float avail = rowWidth > 0.0F ? rowWidth : ImGui::GetContentRegionAvail().x;
        const float fieldWidth = std::max(1.0F, avail - buttonSide - spacing);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_NoUndoRedo;
        if (!revealed) {
            flags |= ImGuiInputTextFlags_Password;
        }
        ImGui::SetNextItemWidth(fieldWidth);
        SecretEdit edit;
        edit.Edited = hint == nullptr
                          ? ImGui::InputText(id, buffer, size, flags)
                          : ImGui::InputTextWithHint(id, hint, buffer, size, flags);
        edit.Submitted = ImGui::IsItemDeactivated() && ImGui::IsKeyDown(ImGuiKey_Enter);
        HoverTooltip(tooltip != nullptr ? tooltip : hint);
        ImGui::SameLine(0.0F, spacing);
        ImGui::PushID(id);
        if (PrimaryButton(revealed ? Icons::EYE_SLASH : Icons::EYE, true, ImVec2(buttonSide, 0.0F))) {
            revealed = !revealed;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", revealed ? Core::Tr("Hide Password") : Core::Tr("Show Password"));
        }
        ImGui::PopID();
        return edit;
    }

    bool BrowseRow(
        const char *fieldId,
        const std::string &path,
        const char *placeholder,
        const char *browseId,
        const char *browseLabel,
        const float rowWidth,
        const char *tooltip
    ) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttonWidth = std::max(
            Em(12.0F),
            ImGui::CalcTextSize(browseLabel).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
        );
        const float avail = rowWidth > 0.0F ? rowWidth : ImGui::GetContentRegionAvail().x;
        const float fieldWidth = std::max(1.0F, avail - buttonWidth - spacing);
        std::vector<char> buffer(std::max<std::size_t>(path.size() + 1, 1));
        std::ranges::copy(path, buffer.begin());
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputTextWithHint(fieldId, placeholder, buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
        HoverTooltip(tooltip);
        ImGui::SameLine();
        const std::string button = Core::StrConcat(browseLabel, "##", browseId);
        return PrimaryButton(button.c_str(), true, ImVec2(buttonWidth, 0.0F));
    }

    float BusyActionWidth(const char *idleLabel, const char *busyLabel) {
        const ImGuiStyle &style = ImGui::GetStyle();
        const float textPad = style.FramePadding.x * 2.0F;
        const float idleW = ImGui::CalcTextSize(idleLabel).x + textPad;
        const float busyW = ImGui::GetTextLineHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(busyLabel).x + textPad;
        return std::max(idleW, busyW) + Em(2.0F);
    }

    bool BusyButton(
        const char *id,
        const char *idleLabel,
        const char *busyLabel,
        const bool busy,
        const bool enabled,
        const float width
    ) {
        const std::string label = Core::StrConcat(busy ? busyLabel : idleLabel, id);
        if (busy) {
            PrimaryButton(label.c_str(), true, ImVec2(width, 0.0F), true);
            return false;
        }
        return PrimaryButton(label.c_str(), enabled, ImVec2(width, 0.0F));
    }

    namespace {
        struct ComboStyle {
            StyleVar Vars;

            ComboStyle() {
                const float s = GetDpiScale();
                Vars.Push(ImGuiStyleVar_PopupRounding, 6.0F * s);
                Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F * s, 12.0F * s));
                Vars.Push(ImGuiStyleVar_FramePadding, ImVec2(14.0F * s, 8.0F * s));
                Vars.Push(ImGuiStyleVar_ItemSpacing, ImVec2(16.0F * s, 12.0F * s));
            }
        };

        bool RoundedSelectable(const char *label, const bool isSelected, const ImGuiSelectableFlags flags = 0, const ImVec2 &size = ImVec2(0, 0)) {
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
    }

    bool RoundedCombo(
        const char *id,
        const char *preview,
        const char *const *labels,
        const int count,
        int &selected,
        const float width,
        const char *tooltip
    ) {
        ImGui::SetNextItemWidth(width);
        bool changed = false;
        {
            const ComboStyle cs;
            if (ImGui::BeginCombo(id, preview != nullptr ? preview : "")) {
                for (int index = 0; index < count; ++index) {
                    const char *label = labels != nullptr && labels[index] != nullptr ? labels[index] : "";
                    ImGui::PushID(index);
                    const bool isSelected = index == selected;
                    if (RoundedSelectable(label, isSelected)) {
                        selected = index;
                        changed = true;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        HoverTooltip(tooltip);
        return changed;
    }

    void SearchField(const char *id, const char *hint, char *buffer, const std::size_t bufferSize) {
        ImGui::SetNextItemWidth(-1.0F);
        const std::string searchHint = IconWithLabel(Icons::SEARCH, hint);
        ImGui::InputTextWithHint(id, searchHint.c_str(), buffer, bufferSize);
    }

    std::string PathPicker(
        const char *id,
        const char *label,
        const char *hint,
        const char *dialogTitle,
        char *buffer,
        const std::size_t bufferSize,
        const float fieldWidth
    ) {
        const char *browseText = Core::Tr("Browse...");
        const std::string browseLabel = Core::StrConcat(browseText, "##", id);
        const float browseWidth = std::max(
            Em(11.0F),
            ImGui::CalcTextSize(browseText).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
        );
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::Text("%s", label);
        const float totalWidth = fieldWidth > 0.0F ? fieldWidth : ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(std::max(1.0F, totalWidth - browseWidth - spacing));
        ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
        ImGui::SameLine();
        if (PrimaryButton(browseLabel.c_str(), true, ImVec2(browseWidth, 0))) {
            if (const auto picked = Core::FileDialog::PickDirectory(dialogTitle, buffer)) {
                CopyToBuffer(buffer, bufferSize, *picked);
            }
        }
        return buffer;
    }

    bool SubtitledCheckbox(const char *id, bool *value, const char *label, const char *subtitle, const char *tooltip, float boxSize) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        const ImGuiStyle &style = ImGui::GetStyle();
        const ImGuiID itemId = window->GetID(id);
        const ImVec2 titleSize = ImGui::CalcTextSize(label, nullptr, true);
        const ImVec2 subSize = subtitle ? ImGui::CalcTextSize(subtitle, nullptr, true) : ImVec2(0, 0);

        const float textBlockH = subtitle ? titleSize.y + style.ItemInnerSpacing.y + subSize.y : titleSize.y;
        const float rowH = ImMax(boxSize, textBlockH);

        const ImVec2 pos = window->DC.CursorPos;
        const float textW = ImMax(titleSize.x, subSize.x);
        const ImVec2 size(boxSize + style.ItemInnerSpacing.x + textW, rowH);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::ItemSize(size, 0.0F);
        if (!ImGui::ItemAdd(bb, itemId)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, itemId, &hovered, &held);
        if (pressed) {
            *value = !*value;
            ImGui::MarkItemEdited(itemId);
        }

        const ImVec2 boxMin(pos.x, pos.y + ((rowH - boxSize) * 0.5F));
        const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
        ImU32 bg = 0;
        if (held && hovered) {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBgActive);
        } else if (hovered) {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
        } else {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
        }
        window->DrawList->AddRectFilled(boxMin, boxMax, bg, style.FrameRounding);
        if (style.FrameBorderSize > 0.0F) {
            window->DrawList->AddRect(
                boxMin,
                boxMax,
                ImGui::GetColorU32(ImGuiCol_Border),
                style.FrameRounding,
                0,
                style.FrameBorderSize
            );
        }
        if (*value) {
            const float pad = ImMax(1.0F, ImFloor(boxSize / 6.0F));
            ImGui::RenderCheckMark(
                window->DrawList,
                ImVec2(boxMin.x + pad, boxMin.y + pad),
                ImGui::GetColorU32(ImGuiCol_CheckMark),
                boxSize - (pad * 2.0F)
            );
        }

        const float textX = boxMax.x + style.ItemInnerSpacing.x;
        if (subtitle) {
            const float textY = pos.y + ((rowH - textBlockH) * 0.5F);
            window->DrawList->AddText(
                ImVec2(textX, textY),
                ImGui::GetColorU32(ImGuiCol_Text),
                label
            );
            window->DrawList->AddText(
                ImVec2(textX, textY + titleSize.y + style.ItemInnerSpacing.y),
                ImGui::GetColorU32(ImGuiCol_TextDisabled),
                subtitle
            );
        } else {
            const float textY = pos.y + ((rowH - titleSize.y) * 0.5F);
            window->DrawList->AddText(
                ImVec2(textX, textY),
                ImGui::GetColorU32(ImGuiCol_Text),
                label
            );
        }

        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", tooltip);
        }

        return pressed;
    }
}

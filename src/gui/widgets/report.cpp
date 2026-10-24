//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <vector>

#include "imgui.h"

#include "../../core/i18n.h"
#include "../widgets.h"

namespace CoreDeck::Gui {
    namespace {
        constexpr const char *UNAVAILABLE = "\u2014";

        void TextDetail(const SplitDetail &detail, const float width, const float labelColumn) {
            const char *shown = detail.Value.empty() ? UNAVAILABLE : detail.Value.c_str();
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float valueWidth = std::max(1.0F, width - labelColumn - spacing);
            const ImVec2 valueSize = ImGui::CalcTextSize(shown, nullptr, false, valueWidth);
            const float rowHeight = std::max(ImGui::GetTextLineHeight(), valueSize.y);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, rowHeight));

            ImDrawList *draw = ImGui::GetWindowDrawList();
            draw->PushClipRect(origin, ImVec2(origin.x + labelColumn, origin.y + rowHeight), true);
            draw->AddText(origin, ImGui::GetColorU32(ImGuiCol_TextDisabled), detail.Label.c_str());
            draw->PopClipRect();
            draw->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize(),
                ImVec2(origin.x + labelColumn + spacing, origin.y),
                ImGui::GetColorU32(ImGuiCol_Text),
                shown,
                nullptr,
                valueWidth
            );
        }

        void ChipDetail(const SplitDetail &detail, const float width, const float labelColumn) {
            const char *shown = detail.Value.empty() ? UNAVAILABLE : detail.Value.c_str();
            const char *accent = detail.Accent != nullptr && detail.Accent[0] != '\0' ? detail.Accent : Colors::TEXT_MUTED;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float padX = 10.0F;
            const float padY = 4.0F;
            const ImVec2 text = ImGui::CalcTextSize(shown);
            const float chipW = text.x + (padX * 2.0F);
            const float chipH = text.y + (padY * 2.0F);
            const float rowH = std::max(ImGui::GetTextLineHeight(), chipH);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, rowH));

            ImDrawList *draw = ImGui::GetWindowDrawList();
            const float labelY = origin.y + ((rowH - ImGui::GetTextLineHeight()) * 0.5F);
            draw->PushClipRect(origin, ImVec2(origin.x + labelColumn, origin.y + rowH), true);
            draw->AddText(ImVec2(origin.x, labelY), ImGui::GetColorU32(ImGuiCol_TextDisabled), detail.Label.c_str());
            draw->PopClipRect();

            const float chipX = origin.x + labelColumn + spacing;
            const float chipY = origin.y + ((rowH - chipH) * 0.5F);
            const ImVec2 chipMin(chipX, chipY);
            const ImVec2 chipMax(chipX + chipW, chipY + chipH);
            const float rounding = chipH * 0.5F;
            const ImU32 ink = ImGui::GetColorU32(HexColor(accent));
            draw->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(HexColor(accent, 0.16F)), rounding);
            draw->AddRect(chipMin, chipMax, ImGui::GetColorU32(HexColor(accent, 0.35F)), rounding, 0, 1.0F);
            draw->AddText(ImVec2(chipX + padX, chipY + ((chipH - text.y) * 0.5F)), ink, shown);
        }

        void SectionDetail(const SplitDetail &detail, const float width) {
            if (detail.Label.empty()) {
                return;
            }
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
            ImGui::Spacing();
            ImGui::TextDisabled("%s", detail.Label.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    void CopyableField(const char *label, const std::string &value) {
        const char *title = label != nullptr ? label : "";
        {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(title);
        }
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttonSide = ImGui::GetFrameHeight();
        const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float fieldWidth = std::max(1.0F, avail - buttonSide - spacing);
        std::vector<char> buffer(std::max<std::size_t>(value.size() + 1, 1), '\0');
        std::ranges::copy(value, buffer.begin());
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText("##Value", buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine(0.0F, spacing);
        ImGui::BeginDisabled(value.empty());
        const std::string tip = Core::TrFormat("Copy {0}", title);
        if (PrimaryButton(Icons::COPY, !value.empty(), ImVec2(buttonSide, 0.0F))) {
            ImGui::SetClipboardText(value.c_str());
        }
        if (!value.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", tip.c_str());
        }
        ImGui::EndDisabled();
    }

    float DetailColumnWidth(const std::span<const char *const> labels, const float avail) {
        float width = 0.0F;
        for (const char *label: labels) {
            if (label == nullptr) {
                continue;
            }
            width = std::max(width, ImGui::CalcTextSize(label).x);
        }
        return std::min(width, std::max(Em(8.0F), avail * 0.45F));
    }

    void DetailLabel(const ImVec2 &origin, const float column, const float rowHeight, const char *label, const float textY) {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, ImVec2(origin.x + column, origin.y + rowHeight), true);
        draw->AddText(ImVec2(origin.x, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), label != nullptr ? label : "");
        draw->PopClipRect();
    }

    void LabeledValue(const char *label, const std::string &value, const std::span<const char *const> columnLabels) {
        constexpr const char *UNAVAILABLE = "\u2014";
        const char *shown = value.empty() ? UNAVAILABLE : value.c_str();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float labelColumn = DetailColumnWidth(columnLabels, avail);
        const float valueWidth = std::max(1.0F, avail - labelColumn - spacing);
        const ImVec2 valueSize = ImGui::CalcTextSize(shown, nullptr, false, valueWidth);
        const float rowHeight = std::max(ImGui::GetTextLineHeight(), valueSize.y);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(avail, rowHeight));

        DetailLabel(origin, labelColumn, rowHeight, label, origin.y);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(origin.x + labelColumn + spacing, origin.y),
            ImGui::GetColorU32(ImGuiCol_Text),
            shown,
            nullptr,
            valueWidth
        );
    }

    void FileRows(const std::string &path, const std::span<const char *const> columnLabels) {
        if (path.empty()) {
            return;
        }
        LabeledValue(Core::Tr("File Name"), FileNameOf(path), columnLabels);
        LabeledValue(Core::Tr("Full Path"), path, columnLabels);
    }

    void ReportHeading(const char *title, const bool *verified) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", title != nullptr ? title : "");
        if (verified == nullptr) {
            return;
        }
        const char *label = *verified ? Core::Tr("Verified") : Core::Tr("Not Verified");
        const char *icon = *verified ? Icons::CHECK_CIRCLE : Icons::TIMES_CIRCLE;
        const float dpi = GetDpiScale();
        const float messageWidth = (16.0F * dpi) + ImGui::CalcTextSize(icon).x + (6.0F * dpi) + ImGui::CalcTextSize(label).x;
        ImGui::SameLine();
        const float remaining = ImGui::GetContentRegionAvail().x;
        if (remaining > messageWidth) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - messageWidth);
        }
        StatusMessage(*verified ? StatusMessageTone::Positive : StatusMessageTone::Error, label);
    }

    void SplitDetailReport(const std::span<const SplitDetail> details, const std::span<const CopyableValue> copies) {
        if (!details.empty()) {
            const int total = static_cast<int>(details.size());
            const int leftCount = (total + 1) / 2;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float column = std::max(1.0F, (ImGui::GetContentRegionAvail().x - spacing) * 0.5F);
            float labelWidth = 0.0F;
            for (const SplitDetail &detail: details) {
                if (detail.Kind == SplitDetailKind::Section || detail.Label.empty()) {
                    continue;
                }
                labelWidth = std::max(labelWidth, ImGui::CalcTextSize(detail.Label.c_str()).x);
            }
            const float labelColumn = std::min(labelWidth, std::max(Em(8.0F), column * 0.45F));
            const auto drawRange = [&](const int begin, const int end) {
                for (int index = begin; index < end; ++index) {
                    const SplitDetail &detail = details[static_cast<std::size_t>(index)]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                    switch (detail.Kind) {
                        case SplitDetailKind::Text:
                            TextDetail(detail, column, labelColumn);
                            break;
                        case SplitDetailKind::Chip:
                            ChipDetail(detail, column, labelColumn);
                            break;
                        case SplitDetailKind::Section:
                            SectionDetail(detail, column);
                            break;
                    }
                }
            };
            ImGui::BeginGroup();
            drawRange(0, leftCount);
            ImGui::EndGroup();
            ImGui::SameLine(0.0F, spacing);
            ImGui::BeginGroup();
            drawRange(leftCount, total);
            ImGui::EndGroup();
        }
        if (copies.empty()) {
            return;
        }
        ImGui::Spacing();
        std::string section;
        for (int index = 0; std::cmp_less(index, copies.size()); ++index) {
            const CopyableValue &copy = copies[static_cast<std::size_t>(index)]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            if (!copy.Section.empty() && copy.Section != section) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", copy.Section.c_str());
                section = copy.Section;
            }
            ImGui::PushID(index);
            ImGui::Spacing();
            CopyableField(copy.Label.c_str(), copy.Value);
            ImGui::PopID();
        }
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
}

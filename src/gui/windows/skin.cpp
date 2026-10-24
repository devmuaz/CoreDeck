//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#include <algorithm>
#include <string>
#include <utility>

#include "imgui.h"

#include "skin.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/utilities.h"
#include "../../core/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        const char *SourceColor(const Core::SkinSource &source) {
            switch (source) {
                case Core::SkinSource::Sdk:
                    return Colors::ACCENT_PHONE;
                case Core::SkinSource::SystemImage:
                    return Colors::POSITIVE;
                case Core::SkinSource::Platform:
                    return Colors::TEXT_SUBTLE;
            }
            return Colors::TEXT_SUBTLE;
        }

        bool MatchesSkinFilter(const Core::Skin &skin, const char *filter) {
            if (!filter || filter[0] == '\0') {
                return true;
            }
            return Core::ContainsIgnoreCase(Core::StrConcat(skin.Name, " ", skin.DisplayName), filter);
        }
    }

    std::string SkinPreviewLabel(const Context &context) {
        const auto &work = context.AvdCreationWork;
        if (work.SelectedSkin <= 0 || work.Skins.empty()) {
            return Core::Tr("No skin (plain emulator window)");
        }
        const int idx = std::clamp(work.SelectedSkin - 1, 0, static_cast<int>(work.Skins.size()) - 1);
        const auto &s = work.Skins.at(idx);
        return Core::StrConcat(s.DisplayName, " - ", Core::SkinSourceLabel(s.Source));
    }


    void BuildSkinWindow(Context &context) {
        if (!context.UI.ShowSkinDialog) {
            return;
        }

        const std::string title = Core::TrWindow("Choose Skin", "SkinDialog");
        if (BeginCenteredModal(title.c_str(), &context.UI.ShowSkinDialog, EmV(80.0F, 25.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            auto &work = context.AvdCreationWork;
            const int totalRows = static_cast<int>(work.Skins.size()) + 1; // +1 for "No skin"
            work.PendingSelectedSkin = std::clamp(work.PendingSelectedSkin, 0, totalRows - 1);

            SearchField("##SkinSearch", Core::Tr("Search skins..."), work.SkinSearchFilter, sizeof(work.SkinSearchFilter));

            ImGui::Spacing();
            ImGui::Text("%s", Core::Tr("Skins"));
            ImGui::Spacing();

            {
                const PickerTableStyle pts;
                const bool tableOpen = BeginPickerTable("##SkinTableFrame", "##SkinTable", 2, Eh(16.0F));
                if (tableOpen) {
                    ImGui::TableSetupColumn(Core::StrConcat("  ", Core::Tr("Name")).c_str(), ImGuiTableColumnFlags_WidthStretch, 2.8F);
                    ImGui::TableSetupColumn(Core::Tr("Source"), ImGuiTableColumnFlags_WidthFixed, Em(15.0F));
                    ImGui::TableHeadersRow();

                    int visibleCount = 0;

                    const bool noSkinSelected = work.PendingSelectedSkin == 0;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string noSkinLabel = Core::StrConcat("  ", Icons::GEAR, "  ", Core::Tr("No skin (plain emulator window)"), "##SkinNone");
                    if (ImGui::Selectable(noSkinLabel.c_str(), noSkinSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                        work.PendingSelectedSkin = 0;
                    }
                    if (noSkinSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("—");
                    visibleCount++;

                    for (int i = 0; std::cmp_less(i, work.Skins.size()); i++) {
                        const auto &skin = work.Skins.at(i);
                        if (!MatchesSkinFilter(skin, work.SkinSearchFilter)) {
                            continue;
                        }

                        const int rowIndex = i + 1;
                        const bool isSelected = work.PendingSelectedSkin == rowIndex;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        const std::string rowLabel = Core::StrConcat("  ", Icons::MOBILE, "  ", skin.DisplayName, "##Skin", std::to_string(i));
                        if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                            work.PendingSelectedSkin = rowIndex;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }

                        ImGui::TableNextColumn();
                        ImGui::TextColored(HexColor(SourceColor(skin.Source)), "%s", Core::SkinSourceLabel(skin.Source));
                        visibleCount++;
                    }

                    if (visibleCount == 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", Core::Tr("No skins match the search."));
                    }
                }
                EndPickerTable(tableOpen);
            }

            if (work.Skins.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", Core::Tr("No skins were found in your SDK. Skins typically ship with system images and the SDK skins folder."));
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            if (PositiveButton(Core::Tr("Use Selected Skin"), true, ImVec2(halfWidth, 0))) {
                work.SelectedSkin = work.PendingSelectedSkin;
                work.SkinAutoFilled = false;
                context.UI.ShowSkinDialog = false;
            }
            ImGui::SameLine();
            if (PrimaryButton(Core::Tr("Cancel"), true, ImVec2(halfWidth, 0))) {
                context.UI.ShowSkinDialog = false;
            }

            ImGui::EndPopup();
        }
    }
}
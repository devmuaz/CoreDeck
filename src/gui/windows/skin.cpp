//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#include <algorithm>
#include <string>

#include "imgui.h"

#include "skin.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    static const char *SourceColor(const SkinSource &source) {
        switch (source) {
            case SkinSource::Sdk:
                return Colors::AccentPhone;
            case SkinSource::SystemImage:
                return Colors::Positive;
            case SkinSource::Platform:
                return Colors::TextSubtle;
        }
        return Colors::TextSubtle;
    }

    std::string SkinPreviewLabel(const Context &context) {
        const auto &work = context.AvdCreationWork;
        if (work.SelectedSkin <= 0 || work.Skins.empty()) {
            return "No skin (plain emulator window)";
        }
        const int idx = std::clamp(work.SelectedSkin - 1, 0, static_cast<int>(work.Skins.size()) - 1);
        const auto &s = work.Skins[idx];
        return StrConcat(s.DisplayName, " - ", SkinSourceLabel(s.Source));
    }

    static bool MatchesSkinFilter(const Skin &skin, const char *filter) {
        if (!filter || filter[0] == '\0') return true;
        return ContainsIgnoreCase(StrConcat(skin.Name, " ", skin.DisplayName), filter);
    }

    void BuildSkinWindow(Context &context) {
        if (!context.UI.ShowSkinDialog) return;

        constexpr auto title = "Choose Skin###SkinDialog";
        if (!ImGui::IsPopupOpen(title)) {
            ImGui::OpenPopup(title);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal(title, &context.UI.ShowSkinDialog, WindowAutoResizeFlags)) {
            auto &work = context.AvdCreationWork;
            const int totalRows = static_cast<int>(work.Skins.size()) + 1; // +1 for "No skin"
            work.PendingSelectedSkin = std::clamp(work.PendingSelectedSkin, 0, totalRows - 1);

            ImGui::SetNextItemWidth(-1.0f);
            const std::string searchHint = IconWithLabel(Icons::Search, "Search skins...");
            ImGui::InputTextWithHint(
                "##SkinSearch",
                searchHint.c_str(),
                work.SkinSearchFilter,
                sizeof(work.SkinSearchFilter)
            );

            ImGui::Spacing();
            ImGui::Text("Skins");
            ImGui::Spacing();

            {
                PickerTableStyle ts;
                ImGui::BeginChild("##SkinTableFrame", ImVec2(-1.0f, 320.0f), true, ImGuiWindowFlags_NoScrollbar);
                if (ImGui::BeginTable("##SkinTable", 2, PickerTableFlags, ImVec2(-1.0f, -1.0f))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("  Name", ImGuiTableColumnFlags_WidthStretch, 2.8f);
                    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                    ImGui::TableHeadersRow();

                    int visibleCount = 0;

                    const bool noSkinSelected = work.PendingSelectedSkin == 0;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string noSkinLabel = StrConcat("  ", Icons::Gear, "  No skin (plain emulator window)##SkinNone");
                    if (ImGui::Selectable(noSkinLabel.c_str(), noSkinSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                        work.PendingSelectedSkin = 0;
                    }
                    if (noSkinSelected) ImGui::SetItemDefaultFocus();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("—");
                    visibleCount++;

                    for (int i = 0; i < static_cast<int>(work.Skins.size()); i++) {
                        const auto &skin = work.Skins[i];
                        if (!MatchesSkinFilter(skin, work.SkinSearchFilter)) continue;

                        const int rowIndex = i + 1;
                        const bool isSelected = work.PendingSelectedSkin == rowIndex;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        const std::string rowLabel = StrConcat("  ", Icons::Mobile, "  ", skin.DisplayName, "##Skin", std::to_string(i));
                        if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                            work.PendingSelectedSkin = rowIndex;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();

                        ImGui::TableNextColumn();
                        ImGui::TextColored(HexColor(SourceColor(skin.Source)), "%s", SkinSourceLabel(skin.Source));
                        visibleCount++;
                    }

                    if (visibleCount == 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("No skins match the search.");
                    }

                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }

            if (work.Skins.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("No skins were found in your SDK. Skins typically ship with system images and the SDK skins folder.");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            if (PrimaryButton("Use Selected Skin", true, ImVec2(halfWidth, 0))) {
                work.SelectedSkin = work.PendingSelectedSkin;
                work.SkinAutoFilled = false;
                context.UI.ShowSkinDialog = false;
            }
            ImGui::SameLine();
            if (PrimaryButton("Cancel", true, ImVec2(halfWidth, 0))) {
                context.UI.ShowSkinDialog = false;
            }

            ImGui::EndPopup();
        }
    }
}
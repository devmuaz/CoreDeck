//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#include <string>

#include "imgui.h"

#include "device_profile.h"
#include "../icons.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    struct DeviceCategoryOption {
        DeviceCategory Category;
        const char *Label;
    };

    DeviceCategory DeviceCategoryForProfile(const DeviceProfile &device) {
        const std::string searchable = LowerCopy(StrConcat(device.Id, " ", device.Name));

        if (searchable.find("wear") != std::string::npos || searchable.find("watch") != std::string::npos) {
            return DeviceCategory::Wear;
        }
        if (searchable.find("automotive") != std::string::npos || searchable.find("auto") != std::string::npos) {
            return DeviceCategory::Automotive;
        }
        if (searchable.find("tv") != std::string::npos) return DeviceCategory::Tv;
        if (searchable.find("desktop") != std::string::npos) return DeviceCategory::Desktop;
        if (searchable.find("tablet") != std::string::npos ||
            searchable.find("fold") != std::string::npos ||
            searchable.find("xl") != std::string::npos) {
            return DeviceCategory::Tablet;
        }
        if (searchable.find("phone") != std::string::npos ||
            searchable.find("pixel") != std::string::npos ||
            searchable.find("nexus") != std::string::npos) {
            return DeviceCategory::Phone;
        }
        return DeviceCategory::Other;
    }

    static LabeledIconStyle DeviceProfileStyleFor(const DeviceProfile &device) {
        switch (DeviceCategoryForProfile(device)) {
            case DeviceCategory::Phone:
                return {Icons::Mobile, "Phone", "#4FC3F7"};
            case DeviceCategory::Tablet:
                return {Icons::Tablet, "Tablet", "#33CC47"};
            case DeviceCategory::Wear:
                return {Icons::Watch, "Wear OS", "#F5A623"};
            case DeviceCategory::Tv:
                return {Icons::Tv, "TV", "#7E57C2"};
            case DeviceCategory::Automotive:
                return {Icons::Car, "Automotive", "#E64D40"};
            case DeviceCategory::Desktop:
                return {Icons::Desktop, "Desktop", "#A7A7AD"};
            case DeviceCategory::All:
            case DeviceCategory::Other:
                return {Icons::Gear, "Other", "#A7A7AD"};
        }
        return {Icons::Gear, "Other", "#A7A7AD"};
    }

    std::string DeviceProfilePreviewLabel(const DeviceProfile &device) {
        const auto [Icon, Label, Color] = DeviceProfileStyleFor(device);
        return StrConcat(device.Name, " - ", Label);
    }

    static bool MatchesDeviceProfileFilters(const DeviceProfile &device, const char *filter, const DeviceCategory category) {
        const bool matchesCategory = category == DeviceCategory::All || DeviceCategoryForProfile(device) == category;
        return matchesCategory && ContainsIgnoreCase(StrConcat(device.Id, " ", device.Name), filter ? filter : "");
    }

    void BuildDeviceProfileWindow(Context &context) {
        if (!context.UI.ShowDeviceProfileDialog) return;

        constexpr auto title = "Choose Device Profile###DeviceProfileDialog";
        if (!ImGui::IsPopupOpen(title)) {
            ImGui::OpenPopup(title);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal(title, &context.UI.ShowDeviceProfileDialog, WindowAutoResizeFlags)) {
            auto &work = context.AvdCreationWork;
            if (!work.DeviceProfiles.empty()) {
                work.PendingSelectedDevice = std::clamp(work.PendingSelectedDevice, 0, static_cast<int>(work.DeviceProfiles.size()) - 1);
            }

            ImGui::SetNextItemWidth(-1.0f);
            const std::string searchHint = IconWithLabel(Icons::Search, "Search device profiles...");
            ImGui::InputTextWithHint(
                "##DeviceProfileSearch",
                searchHint.c_str(),
                work.DeviceSearchFilter,
                sizeof(work.DeviceSearchFilter)
            );

            ImGui::Spacing();
            ImGui::TextDisabled("Categories");

            static constexpr DeviceCategoryOption categoryOptions[] = {
                {DeviceCategory::All, "All"},
                {DeviceCategory::Phone, "Phone"},
                {DeviceCategory::Tablet, "Tablet"},
                {DeviceCategory::Wear, "Wear OS"},
                {DeviceCategory::Tv, "TV"},
                {DeviceCategory::Automotive, "Automotive"},
                {DeviceCategory::Desktop, "Desktop"},
                {DeviceCategory::Other, "Other"},
            };

            bool firstCategory = true;
            for (const auto &[Category, Label]: categoryOptions) {
                if (!firstCategory) ImGui::SameLine();
                firstCategory = false;
                if (CategoryChip(Label, work.SelectedDeviceCategory == Category)) {
                    work.SelectedDeviceCategory = Category;
                }
            }

            ImGui::Spacing();
            ImGui::Text("Device Profiles");
            ImGui::Spacing();

            {
                PickerTableStyle tableStyle;

                ImGui::BeginChild("##DeviceProfileTableFrame", ImVec2(-1.0f, 280.0f), true, ImGuiWindowFlags_NoScrollbar);
                if (ImGui::BeginTable("##DeviceProfileTable", 2, PickerTableFlags, ImVec2(-1.0f, -1.0f))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("  Name", ImGuiTableColumnFlags_WidthStretch, 2.8f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                    ImGui::TableHeadersRow();

                    int visibleCount = 0;
                    for (int i = 0; i < static_cast<int>(work.DeviceProfiles.size()); i++) {
                        const auto &device = work.DeviceProfiles[i];
                        if (!MatchesDeviceProfileFilters(device, work.DeviceSearchFilter, work.SelectedDeviceCategory)) {
                            continue;
                        }

                        visibleCount++;
                        const bool isSelected = work.PendingSelectedDevice == i;
                        const auto [Icon, Label, Color] = DeviceProfileStyleFor(device);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        const std::string rowLabel = StrConcat("  ", Icon, "  ", device.Name, "##DeviceProfile", std::to_string(i));
                        if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                            work.PendingSelectedDevice = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();

                        ImGui::TableNextColumn();
                        ImGui::TextColored(HexColor(Color), "%s", Label);
                    }

                    if (visibleCount == 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("No device profiles match the selected form factor and search.");
                    }

                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (PrimaryButton("Use Selected Device", !work.DeviceProfiles.empty(), ImVec2(halfWidth, 0))) {
                work.SelectedDevice = work.PendingSelectedDevice;
                context.UI.ShowDeviceProfileDialog = false;
            }
            ImGui::SameLine();
            if (PrimaryButton("Cancel", true, ImVec2(halfWidth, 0))) {
                context.UI.ShowDeviceProfileDialog = false;
            }

            ImGui::EndPopup();
        }
    }
}

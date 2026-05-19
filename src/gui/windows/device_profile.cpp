//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#include <string>

#include "imgui.h"

#include "device_profile.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/utilities.h"

namespace CoreDeck {

    DeviceCategory DeviceCategoryForProfile(const DeviceProfile &device) {
        const std::string searchable = LowerCopy(StrConcat(device.Id, " ", device.Name));

        if (searchable.find("wear") != std::string::npos || searchable.find("watch") != std::string::npos) {
            return DeviceCategory::Wear;
        }
        if (searchable.find("automotive") != std::string::npos || searchable.find("auto") != std::string::npos) {
            return DeviceCategory::Automotive;
        }
        if (searchable.find("tv") != std::string::npos) {
            return DeviceCategory::Tv;
        }
        if (searchable.find("desktop") != std::string::npos) {
            return DeviceCategory::Desktop;
        }
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

    namespace {
        struct DeviceCategoryOption {
            DeviceCategory Category;
            const char *Label;
        };

        LabeledIconStyle DeviceProfileStyleFor(const DeviceProfile &device) {
            switch (DeviceCategoryForProfile(device)) {
                case DeviceCategory::Phone:
                    return {.Icon = Icons::MOBILE, .Label = "Phone", .Color = Colors::ACCENT_PHONE};
                case DeviceCategory::Tablet:
                    return {.Icon = Icons::TABLET, .Label = "Tablet", .Color = Colors::ACCENT_TABLET};
                case DeviceCategory::Wear:
                    return {.Icon = Icons::WATCH, .Label = "Wear OS", .Color = Colors::ACCENT_WEAR};
                case DeviceCategory::Tv:
                    return {.Icon = Icons::TV, .Label = "TV", .Color = Colors::ACCENT_TV};
                case DeviceCategory::Automotive:
                    return {.Icon = Icons::CAR, .Label = "Automotive", .Color = Colors::NEGATIVE};
                case DeviceCategory::Desktop:
                    return {.Icon = Icons::DESKTOP, .Label = "Desktop", .Color = Colors::TEXT_SUBTLE};
                case DeviceCategory::All:
                case DeviceCategory::Other:
                    return {.Icon = Icons::GEAR, .Label = "Other", .Color = Colors::TEXT_SUBTLE};
            }
            return {.Icon = Icons::GEAR, .Label = "Other", .Color = Colors::TEXT_SUBTLE};
        }

        bool MatchesDeviceProfileFilters(const DeviceProfile &device, const char *filter, const DeviceCategory category) {
            const bool matchesCategory = category == DeviceCategory::All || DeviceCategoryForProfile(device) == category;
            return matchesCategory && ContainsIgnoreCase(StrConcat(device.Id, " ", device.Name), filter ? filter : "");
        }
    }


    std::string DeviceProfilePreviewLabel(const DeviceProfile &device) {
        const auto [Icon, Label, Color] = DeviceProfileStyleFor(device);
        return StrConcat(device.Name, " - ", Label);
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildDeviceProfileWindow(Context &context) {
        if (!context.UI.ShowDeviceProfileDialog) {
            return;
        }

        constexpr auto TITLE = "Choose Device Profile###DeviceProfileDialog";
        if (!ImGui::IsPopupOpen(TITLE)) {
            ImGui::OpenPopup(TITLE);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(EmV(84.0F, 26.0F), ImGuiCond_Appearing);

        if (RoundedBeginPopupModal(TITLE, &context.UI.ShowDeviceProfileDialog, WINDOW_AUTO_RESIZE_FLAGS)) {
            auto &work = context.AvdCreationWork;
            if (!work.DeviceProfiles.empty()) {
                work.PendingSelectedDevice = std::clamp(work.PendingSelectedDevice, 0, static_cast<int>(work.DeviceProfiles.size()) - 1);
            }

            ImGui::SetNextItemWidth(-1.0F);
            const std::string searchHint = IconWithLabel(Icons::SEARCH, "Search device profiles...");
            ImGui::InputTextWithHint(
                "##DeviceProfileSearch",
                searchHint.c_str(),
                work.DeviceSearchFilter,
                sizeof(work.DeviceSearchFilter)
            );

            ImGui::Spacing();
            ImGui::TextDisabled("Categories");

            static constexpr DeviceCategoryOption CATEGORY_OPTIONS[] = {
                {.Category = DeviceCategory::All, .Label = "All"},
                {.Category = DeviceCategory::Phone, .Label = "Phone"},
                {.Category = DeviceCategory::Tablet, .Label = "Tablet"},
                {.Category = DeviceCategory::Wear, .Label = "Wear OS"},
                {.Category = DeviceCategory::Tv, .Label = "TV"},
                {.Category = DeviceCategory::Automotive, .Label = "Automotive"},
                {.Category = DeviceCategory::Desktop, .Label = "Desktop"},
                {.Category = DeviceCategory::Other, .Label = "Other"},
            };

            bool firstCategory = true;
            for (const auto &[Category, Label]: CATEGORY_OPTIONS) {
                if (!firstCategory) {
                    ImGui::SameLine();
                }
                firstCategory = false;
                if (CategoryChip(Label, work.SelectedDeviceCategory == Category)) {
                    work.SelectedDeviceCategory = Category;
                }
            }

            ImGui::Spacing();
            ImGui::Text("Device Profiles");
            ImGui::Spacing();

            {
                PickerTableStyle pts;

                ImGui::BeginChild("##DeviceProfileTableFrame", ImVec2(-1.0F, Eh(14.0F)), 1, ImGuiWindowFlags_NoScrollbar);
                if (ImGui::BeginTable("##DeviceProfileTable", 2, PICKER_TABLE_FLAGS, ImVec2(-1.0F, -1.0F))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("  Name", ImGuiTableColumnFlags_WidthStretch, 2.8F);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Em(14.0F));
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
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }

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
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5F;
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

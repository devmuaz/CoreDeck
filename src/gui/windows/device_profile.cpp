//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#include <string>

#include "imgui.h"

#include "device_profile.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/utilities.h"
#include "../../core/i18n.h"

namespace CoreDeck {

    DeviceCategory DeviceCategoryForText(const std::string &text) {
        const std::string searchable = LowerCopy(text);

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
            searchable.find("xl") != std::string::npos ||
            searchable.find("pixel_c") != std::string::npos) {
            return DeviceCategory::Tablet;
        }
        if (searchable.find("phone") != std::string::npos ||
            searchable.find("pixel") != std::string::npos ||
            searchable.find("nexus") != std::string::npos) {
            return DeviceCategory::Phone;
        }
        return DeviceCategory::Other;
    }

    DeviceCategory DeviceCategoryForProfile(const DeviceProfile &device) {
        return DeviceCategoryForText(StrConcat(device.Id, " ", device.Name));
    }

    LabeledIconStyle DeviceFormFactorStyle(const DeviceCategory category) {
        switch (category) {
            case DeviceCategory::Phone:
                return {.Icon = Icons::MOBILE, .Label = Tr("Phone"), .Color = Colors::ACCENT_PHONE};
            case DeviceCategory::Tablet:
                return {.Icon = Icons::TABLET, .Label = Tr("Tablet"), .Color = Colors::ACCENT_TABLET};
            case DeviceCategory::Wear:
                return {.Icon = Icons::WATCH, .Label = Tr("Wear OS"), .Color = Colors::ACCENT_WEAR};
            case DeviceCategory::Tv:
                return {.Icon = Icons::TV, .Label = Tr("TV"), .Color = Colors::ACCENT_TV};
            case DeviceCategory::Automotive:
                return {.Icon = Icons::CAR, .Label = Tr("Automotive"), .Color = Colors::NEGATIVE};
            case DeviceCategory::Desktop:
                return {.Icon = Icons::DESKTOP, .Label = Tr("Desktop"), .Color = Colors::TEXT_SUBTLE};
            case DeviceCategory::All:
            case DeviceCategory::Other:
                return {.Icon = Icons::GEAR, .Label = Tr("Other"), .Color = Colors::TEXT_SUBTLE};
        }
        return {.Icon = Icons::GEAR, .Label = Tr("Other"), .Color = Colors::TEXT_SUBTLE};
    }

    namespace {
        bool MatchesDeviceProfileFilters(const DeviceProfile &device, const char *filter, const DeviceCategory category) {
            const bool matchesCategory = category == DeviceCategory::All || DeviceCategoryForProfile(device) == category;
            return matchesCategory && ContainsIgnoreCase(StrConcat(device.Id, " ", device.Name), filter ? filter : "");
        }
    }


    std::string DeviceProfilePreviewLabel(const DeviceProfile &device) {
        const auto style = DeviceFormFactorStyle(DeviceCategoryForProfile(device));
        return StrConcat(device.Name, " - ", style.Label);
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildDeviceProfileWindow(Context &context) {
        if (!context.UI.ShowDeviceProfileDialog) {
            return;
        }

        const std::string title = TrWindow("Choose Device Profile", "DeviceProfileDialog");
        if (BeginCenteredModal(title.c_str(), &context.UI.ShowDeviceProfileDialog, EmV(84.0F, 26.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            auto &work = context.AvdCreationWork;
            if (!work.DeviceProfiles.empty()) {
                work.PendingSelectedDevice = std::clamp(work.PendingSelectedDevice, 0, static_cast<int>(work.DeviceProfiles.size()) - 1);
            }

            SearchField("##DeviceProfileSearch", Tr("Search device profiles..."), work.DeviceSearchFilter, sizeof(work.DeviceSearchFilter));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", Tr("Categories"));

            const char *categoryLabels[] = {
                Tr("All"),
                Tr("Phone"),
                Tr("Tablet"),
                Tr("Wear OS"),
                Tr("TV"),
                Tr("Automotive"),
                Tr("Desktop"),
                Tr("Other"),
            };
            static_assert(IM_ARRAYSIZE(categoryLabels) == static_cast<int>(DeviceCategory::Other) + 1);
            int selectedCategory = static_cast<int>(work.SelectedDeviceCategory);
            if (CategoryChipRow(categoryLabels, IM_ARRAYSIZE(categoryLabels), selectedCategory)) {
                work.SelectedDeviceCategory = static_cast<DeviceCategory>(selectedCategory);
            }

            ImGui::Spacing();
            ImGui::Text("%s", Tr("Device Profiles"));
            ImGui::Spacing();

            {
                PickerTableStyle pts;
                const bool tableOpen = BeginPickerTable("##DeviceProfileTableFrame", "##DeviceProfileTable", 2, Eh(14.0F));
                if (tableOpen) {
                    const std::string nameColumn = StrConcat("  ", Tr("Name"));
                    ImGui::TableSetupColumn(nameColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 2.8F);
                    ImGui::TableSetupColumn(Tr("Type"), ImGuiTableColumnFlags_WidthFixed, Em(14.0F));
                    ImGui::TableHeadersRow();

                    int visibleCount = 0;
                    for (int i = 0; i < static_cast<int>(work.DeviceProfiles.size()); i++) {
                        const auto &device = work.DeviceProfiles.at(static_cast<std::size_t>(i));
                        if (!MatchesDeviceProfileFilters(device, work.DeviceSearchFilter, work.SelectedDeviceCategory)) {
                            continue;
                        }

                        visibleCount++;
                        const bool isSelected = work.PendingSelectedDevice == i;
                        const auto style = DeviceFormFactorStyle(DeviceCategoryForProfile(device));

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        const std::string rowLabel = StrConcat("  ", style.Icon, "  ", device.Name, "##DeviceProfile", std::to_string(i));
                        if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                            work.PendingSelectedDevice = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }

                        ImGui::TableNextColumn();
                        ImGui::TextColored(HexColor(style.Color), "%s", style.Label);
                    }

                    if (visibleCount == 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", Tr("No device profiles match the selected form factor and search."));
                    }
                }
                EndPickerTable(tableOpen);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);
            if (PositiveButton(Tr("Use Selected Device"), !work.DeviceProfiles.empty(), ImVec2(halfWidth, 0))) {
                work.SelectedDevice = work.PendingSelectedDevice;
                work.DeviceAutoFilled = false;
                context.UI.ShowDeviceProfileDialog = false;
            }
            ImGui::SameLine();
            if (PrimaryButton(Tr("Cancel"), true, ImVec2(halfWidth, 0))) {
                context.UI.ShowDeviceProfileDialog = false;
            }

            ImGui::EndPopup();
        }
    }
}

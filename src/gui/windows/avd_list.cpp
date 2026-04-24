//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//
#include <algorithm>
#include "imgui.h"

#include "avd_list.h"
#include "delete_avd.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../icons.h"

namespace CoreDeck {
    struct DeviceIconStyle {
        const char *Icon;
        const char *HexColor;
    };

    static DeviceIconStyle DeviceIconStyleFor(const std::string &device) {
        std::string d;
        d.reserve(device.size());
        for (const char c: device) d.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        if (d.find("wear") != std::string::npos) return {Icons::Watch, "#F5A623"};
        if (d.find("auto") != std::string::npos) return {Icons::Car, "#E64D40"};
        if (d.find("tv") != std::string::npos) return {Icons::Tv, "#7E57C2"};
        if (d.find("tablet") != std::string::npos || d.find("pixel_c") != std::string::npos)
            return {Icons::Tablet, "#33CC47"};
        return {Icons::Mobile, "#4FC3F7"};
    }

    static bool ContainsCaseInsensitive(const std::string &haystack, const char *needle) {
        if (needle[0] == '\0') return true;

        const auto len = std::strlen(needle);
        if (len > haystack.size()) return false;

        return std::search(
                   haystack.begin(), haystack.end(),
                   needle, needle + len,
                   [](const char a, const char b) {
                       return std::tolower(static_cast<unsigned char>(a)) ==
                              std::tolower(static_cast<unsigned char>(b));
                   }
               ) != haystack.end();
    }

    static void RebuildFilteredIndices(Context &context) {
        auto &catalog = context.Catalog;
        catalog.FilteredIndices.clear();

        // Filter
        for (int i = 0; i < static_cast<int>(catalog.Avds.size()); i++) {
            const auto &avd = catalog.Avds[i];
            if (catalog.SearchFilter[0] != '\0') {
                if (!ContainsCaseInsensitive(avd.DisplayName, catalog.SearchFilter) &&
                    !ContainsCaseInsensitive(avd.Name, catalog.SearchFilter) &&
                    !ContainsCaseInsensitive(avd.Device, catalog.SearchFilter) &&
                    !ContainsCaseInsensitive(avd.ApiLevel, catalog.SearchFilter)) {
                    continue;
                }
            }
            catalog.FilteredIndices.push_back(i);
        }

        // Sort
        const bool asc = catalog.SortAscending;
        std::ranges::sort(catalog.FilteredIndices, [&](const int a, const int b) {
            const auto &avdA = catalog.Avds[a];
            const auto &avdB = catalog.Avds[b];
            int cmp = 0;

            switch (catalog.SortMode) {
                case AvdSortMode::Name: {
                    auto lowerA = avdA.DisplayName;
                    auto lowerB = avdB.DisplayName;
                    for (auto &c: lowerA) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    for (auto &c: lowerB) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    cmp = lowerA.compare(lowerB);
                    break;
                }
                case AvdSortMode::ApiLevel: {
                    const int apiA = static_cast<int>(std::strtol(avdA.ApiLevel.c_str(), nullptr, 10));
                    const int apiB = static_cast<int>(std::strtol(avdB.ApiLevel.c_str(), nullptr, 10));
                    cmp = apiA - apiB;
                    break;
                }
                case AvdSortMode::Device: {
                    cmp = avdA.Device.compare(avdB.Device);
                    break;
                }
            }
            return asc ? cmp < 0 : cmp > 0;
        });
    }

    static constexpr const char *SortModeLabels[] = {"Name", "API Level", "Device"};
    static constexpr int SortModeCount = 3;

    void BuildAvdListWindow(Context &context) {
        if (!context.UI.ShowAvdListPanel) return;

        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Available AVDs (Android Virtual Device)###AVDs", nullptr, panelFlags);

        auto openCreateAvdDialog = [&context] {
            context.AvdCreationWork.CreationData = {};
            context.AvdCreationWork.SelectedSystemImage = 0;
            context.AvdCreationWork.SelectedDevice = 0;
            context.AvdCreationWork.SelectedGpuMode = 0;
            context.AvdCreationWork.NameAutoFilled = true;
            context.AvdCreationWork.DisplayNameAutoFilled = true;
            context.AvdCreationWork.Prefetch.Ready = false;
            context.AvdCreationWork.Prefetch.Loading = true;
            context.UI.ShowCreateAvdDialog = true;

            context.AvdCreationWork.Prefetch.Future = std::async(std::launch::async, [&context] {
                auto images = ListSystemImages(context.Host.Sdk);
                auto devices = ListDeviceProfiles(context.Host.Sdk);
                context.AvdCreationWork.SystemImages = std::move(images);
                context.AvdCreationWork.DeviceProfiles = std::move(devices);
                context.AvdCreationWork.Prefetch.Loading = false;
                context.AvdCreationWork.Prefetch.Ready = true;
            });
        };

        if (PrimaryButton(Icons::Refresh)) RefreshAvds(context);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh the AVD list");

        ImGui::SameLine();

        if (PrimaryButton(Icons::Plus)) openCreateAvdDialog();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new AVD");

        if (context.Catalog.SelectedAvd >= 0) {
            const auto &avd = context.Catalog.Avds[context.Catalog.SelectedAvd];
            const bool isRunning = context.Host.Manager.IsRunning(avd.Name);
            const auto args = BuildArgs(avd.Name, GetDefaultAvdOptions(context));

            ImGui::SameLine();
            if (isRunning) {
                if (NegativeButton(IconWithLabel(Icons::Stop, "Stop").c_str())) {
                    context.Host.Manager.Stop(avd.Name);
                }
            } else {
                if (PositiveButton(Icons::Play)) {
                    context.Host.Manager.Launch(avd.Name, args);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run the selected AVD");
                ImGui::SameLine();
                if (WarningButton(IconWithLabel(Icons::Terminal, "Wipe & Run").c_str())) {
                    auto wipeArgs = args;
                    wipeArgs.emplace_back("-wipe-data");
                    context.Host.Manager.Launch(avd.Name, wipeArgs);
                }
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
                ImGui::SameLine(0, 15.0f);
                if (PrimaryButton(Icons::Trash)) {
                    if (context.Prefs.ConfirmBeforeDeleteAvd) {
                        context.UI.ShowDeleteAvdDialog = true;
                    } else {
                        StartDeleteAvdAsync(context, avd.Name);
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete currently selected AVD");
            }

            if (isRunning) {
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
            }
        }

        ImGui::Separator();

        if (context.Catalog.Avds.empty()) {
            ImGui::TextDisabled("No AVDs found");
            ImGui::End();
            return;
        }

        ImGui::Spacing(); {
            const char *sortDirIcon = context.Catalog.SortAscending ? Icons::SortUp : Icons::SortDown;
            const char *sortDirTooltip = context.Catalog.SortAscending ? "Ascending" : "Descending";
            if (PrimaryButton(sortDirIcon)) {
                context.Catalog.SortAscending = !context.Catalog.SortAscending;
                PersistAppSettings(context);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", sortDirTooltip);

            ImGui::SameLine();

            ImGui::SetNextItemWidth(150.0f);
            const int currentSortIdx = static_cast<int>(context.Catalog.SortMode);
            if (ImGui::BeginCombo("##AvdSort", SortModeLabels[currentSortIdx])) {
                for (int i = 0; i < SortModeCount; i++) {
                    const bool selected = currentSortIdx == i;
                    if (ImGui::Selectable(SortModeLabels[i], selected)) {
                        context.Catalog.SortMode = static_cast<AvdSortMode>(i);
                        PersistAppSettings(context);
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            const auto searchHint = IconWithLabel(Icons::Search, "Search AVDs...");
            ImGui::InputTextWithHint(
                "##AvdSearch",
                searchHint.c_str(),
                context.Catalog.SearchFilter,
                IM_ARRAYSIZE(context.Catalog.SearchFilter)
            );
        }
        ImGui::Spacing();

        RebuildFilteredIndices(context);

        const auto &filtered = context.Catalog.FilteredIndices;
        if (context.Catalog.SelectedAvd >= 0) {
            bool selectionVisible = false;
            for (const int idx: filtered) {
                if (idx == context.Catalog.SelectedAvd) {
                    selectionVisible = true;
                    break;
                }
            }
            if (!selectionVisible && !filtered.empty()) {
                context.Catalog.SelectedAvd = filtered[0];
            }
        }

        if (filtered.empty()) {
            ImGui::TextDisabled("No matching AVDs");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("AvdList", ImVec2(0, 0), ImGuiChildFlags_None);
        for (const int i: filtered) {
            const auto &avd = context.Catalog.Avds[i];
            const bool isSelected = context.Catalog.SelectedAvd == i;
            const bool isRunning = context.Host.Manager.IsRunning(avd.Name);

            ImGui::PushID(i);
            const char *avdStatusText = isRunning ? "Running..." : "Ready";
            const ImVec4 avdStatusColor = isRunning ? HexColor("#33CC47") : HexColor("#66666B");
            const DeviceIconStyle iconStyle = DeviceIconStyleFor(avd.Device);
            if (SelectableItem(avd.DisplayName.c_str(), isSelected, avdStatusText, avdStatusColor,
                               iconStyle.Icon, HexColor(iconStyle.HexColor))) {
                context.Catalog.SelectedAvd = i;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//
#include <algorithm>
#include <utility>
#include "imgui.h"

#include "avd_list.h"
#include "avd_info.h"
#include "delete_avd.h"
#include "device_profile.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "platform/utilities.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void RebuildFilteredIndices(Context &context) {
            auto &catalog = context.Catalog;
            catalog.FilteredIndices.clear();

            // Filter
            for (int i = 0; std::cmp_less(i, catalog.Avds.size()); i++) {
                const auto &avd = catalog.Avds.at(i);
                if (catalog.SearchFilter[0] != '\0' && !Platform::ContainsIgnoreCase(avd.DisplayName, catalog.SearchFilter) &&
                    !Platform::ContainsIgnoreCase(avd.Name, catalog.SearchFilter) &&
                    !Platform::ContainsIgnoreCase(avd.Device, catalog.SearchFilter) &&
                    !Platform::ContainsIgnoreCase(avd.ApiLevel, catalog.SearchFilter) &&
                    !Platform::ContainsIgnoreCase(SystemImageKindLabel(avd), catalog.SearchFilter)) {
                    continue;
                }

                catalog.FilteredIndices.push_back(i);
            }

            // Sort
            const bool asc = catalog.SortAscending;
            std::ranges::sort(catalog.FilteredIndices, [&](const int a, const int b) {
                const auto &avdA = catalog.Avds.at(a);
                const auto &avdB = catalog.Avds.at(b);
                int cmp = 0;

                switch (catalog.SortMode) {
                    case AvdSortMode::Name: {
                        auto lowerA = avdA.DisplayName;
                        auto lowerB = avdB.DisplayName;
                        for (auto &c: lowerA) {
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        }
                        for (auto &c: lowerB) {
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        }
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

        constexpr int SORT_MODE_COUNT = 3;

        void OpenCreateAvdDialog(Context &context) {
            context.AvdCreationWork.CreationData = {};
            context.AvdCreationWork.SelectedSystemImage = 0;
            context.AvdCreationWork.SelectedDevice = 0;
            context.AvdCreationWork.SelectedGpuMode = 0;
            context.AvdCreationWork.NameAutoFilled = true;
            context.AvdCreationWork.DisplayNameAutoFilled = true;
            context.AvdCreationWork.DeviceAutoFilled = true;
            context.AvdCreationWork.PendingSelectedDevice = 0;
            context.AvdCreationWork.SkinAutoFilled = true;
            context.AvdCreationWork.SelectedSkin = 0;
            context.AvdCreationWork.PendingSelectedSkin = 0;
            context.AvdCreationWork.LastDeviceForSkinAuto = -1;
            context.AvdCreationWork.SkinSearchFilter[0] = '\0';
            context.AvdCreationWork.DeviceDefinitionsSkipped = false;
            context.AvdCreationWork.Prefetch.Ready = false;
            context.AvdCreationWork.Prefetch.Loading = true;
            context.UI.ShowCreateAvdDialog = true;

            context.AvdCreationWork.Prefetch.Future = std::async(std::launch::async, [&context] {
                auto images = Core::ListSystemImages(context.Host.Sdk);
                auto devices = Core::ListDeviceProfiles(context.Host.Sdk);
                auto skins = Core::ListSkins(context.Host.Sdk);
                context.AvdCreationWork.SystemImages = std::move(images);
                context.AvdCreationWork.DeviceDefinitionsSkipped = devices.SkippedDeviceDefinitions;
                context.AvdCreationWork.DeviceProfiles = std::move(devices.Profiles);
                context.AvdCreationWork.Skins = std::move(skins);
                context.AvdCreationWork.Prefetch.Loading = false;
                context.AvdCreationWork.Prefetch.Ready = true;
            });
        }

    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildAvdListWindow(Context &context) {
        if (!context.UI.ShowAvdListPanel) {
            return;
        }

        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        const std::string windowTitle = Platform::TrWindow("Available AVDs (Android Virtual Device)", "AVDs");
        ImGui::Begin(windowTitle.c_str(), nullptr, FLAGS);

        if (PrimaryButton(Icons::REFRESH)) {
            RequestAvdListRefresh(context);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Platform::Tr("Refresh the AVD list"));
        }

        ImGui::SameLine();

        if (PrimaryButton(Icons::PLUS)) {
            OpenCreateAvdDialog(context);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Platform::Tr("Create new AVD"));
        }

        if (context.Catalog.SelectedAvd >= 0) {
            const auto &avd = context.Catalog.Avds.at(context.Catalog.SelectedAvd);
            const bool isRunning = context.Host.Manager.IsRunning(avd.Name);
            const auto args = Core::BuildArgs(avd.Name, GetDefaultAvdOptions(context));

            ImGui::SameLine();
            if (isRunning) {
                const bool isStopping = context.Host.Manager.IsStopping(avd.Name);
                const std::string label = IconWithLabel(Icons::STOP, isStopping ? Platform::Tr("Stopping...") : Platform::Tr("Stop"));
                if (NegativeButton(label.c_str(), !isStopping) && !isStopping) {
                    context.Host.Manager.Stop(avd.Name);
                }
            } else {
                if (PositiveButton(Icons::PLAY)) {
                    context.Host.Manager.Launch(avd.Name, args);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", Platform::Tr("Run the selected AVD"));
                }
                ImGui::SameLine();
                if (WarningButton(IconWithLabel(Icons::TERMINAL, Platform::Tr("Wipe & Run")).c_str())) {
                    auto wipeArgs = args;
                    wipeArgs.emplace_back("-wipe-data");
                    context.Host.Manager.Launch(avd.Name, wipeArgs);
                }
                ImGui::SameLine();
                if (NegativeButton(Icons::TRASH)) {
                    if (context.Prefs.ConfirmBeforeDeleteAvd) {
                        context.UI.ShowDeleteAvdDialog = true;
                    } else {
                        StartDeleteAvdAsync(context, avd.Name);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", Platform::Tr("Delete currently selected AVD"));
                }
            }
        }

        ImGui::Separator();

        if (context.Catalog.Avds.empty()) {
            const std::string createLabel = IconWithLabel(Icons::PLUS, Platform::Tr("Create AVD"));
            if (NoticeCard(
                    NoticeCardTone::Default,
                    Platform::Tr("No Android Virtual Devices Found"),
                    Platform::Tr("Only Android Virtual Devices (AVDs) with a system image installed in this SDK are listed."),
                    createLabel.c_str()
                )) {
                OpenCreateAvdDialog(context);
            }
            ImGui::End();
            return;
        }

        ImGui::Spacing();
        const char *sortDirIcon = context.Catalog.SortAscending ? Icons::SORT_UP : Icons::SORT_DOWN;
        const char *sortDirTooltip = context.Catalog.SortAscending ? Platform::Tr("Ascending") : Platform::Tr("Descending");
        if (PrimaryButton(sortDirIcon)) {
            context.Catalog.SortAscending = !context.Catalog.SortAscending;
            PersistAppSettings(context);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", sortDirTooltip);
        }

        ImGui::SameLine();

        int currentSortIdx = static_cast<int>(context.Catalog.SortMode);
        const char *const sortModeLabels[] = {Platform::Tr("Name"), Platform::Tr("API Level"), Platform::Tr("Device")};
        if (RoundedCombo("##AvdSort", sortModeLabels[currentSortIdx], sortModeLabels, SORT_MODE_COUNT, currentSortIdx, Em(17.0F))) {
            context.Catalog.SortMode = static_cast<AvdSortMode>(currentSortIdx);
            PersistAppSettings(context);
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        const auto searchHint = IconWithLabel(Icons::SEARCH, Platform::Tr("Search AVDs..."));
        ImGui::InputTextWithHint(
            "##AvdSearch",
            searchHint.c_str(),
            context.Catalog.SearchFilter,
            IM_ARRAYSIZE(context.Catalog.SearchFilter)
        );
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
                context.Catalog.SelectedAvd = filtered.at(0);
            }
        }

        if (filtered.empty()) {
            ImGui::TextDisabled("%s", Platform::Tr("No matching AVDs"));
            ImGui::End();
            return;
        }

        ImGui::BeginChild("AvdList", ImVec2(0, 0), ImGuiChildFlags_None);
        for (const int i: filtered) {
            const auto &avd = context.Catalog.Avds.at(i);
            const bool isSelected = context.Catalog.SelectedAvd == i;
            const bool isRunning = context.Host.Manager.IsRunning(avd.Name);

            ImGui::PushID(i);
            const char *avdStatusText = isRunning ? Platform::Tr("Running...") : Platform::Tr("Ready");
            const ImVec4 avdStatusColor = isRunning ? HexColor(Colors::POSITIVE) : HexColor(Colors::TEXT_MUTED);
            const std::string avdRightText = Platform::StrConcat(SystemImageKindLabel(avd), " - ", avdStatusText);
            const auto style = DeviceFormFactorStyle(DeviceCategoryForText(avd.Device));
            if (SelectableItem(avd.DisplayName.c_str(), isSelected, avdRightText.c_str(), avdStatusColor, style.Icon, HexColor(style.Color))) {
                context.Catalog.SelectedAvd = i;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//
#include "imgui.h"

#include "avd_list.h"
#include "delete_avd.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    void BuildAvdListWindow(Context &context) {
        if (!context.UI.ShowAvdListPanel) return;

        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Available AVDs (Android Virtual Device)###AVDs", nullptr, panelFlags);

        if (PrimaryButton(Icons::Refresh)) RefreshAvds(context);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh the AVD list");

        ImGui::SameLine();


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

            ImGui::SameLine();
            if (PrimaryButton(Icons::Plus)) {
                context.CreateAvdWork.CreateParams = {};
                context.CreateAvdWork.SelectedSystemImage = 0;
                context.CreateAvdWork.SelectedDevice = 0;
                context.CreateAvdWork.SelectedGpuMode = 0;
                context.CreateAvdWork.Prefetch.Ready = false;
                context.CreateAvdWork.Prefetch.Loading = true;
                context.UI.ShowCreateAvdDialog = true;

                context.CreateAvdWork.Prefetch.Future = std::async(std::launch::async, [&context] {
                    auto images = ListSystemImages(context.Host.Sdk);
                    auto devices = ListDeviceProfiles(context.Host.Sdk);
                    context.CreateAvdWork.SystemImages = std::move(images);
                    context.CreateAvdWork.DeviceProfiles = std::move(devices);
                    context.CreateAvdWork.Prefetch.Loading = false;
                    context.CreateAvdWork.Prefetch.Ready = true;
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new AVD");
        }

        ImGui::Separator();

        if (context.Catalog.Avds.empty()) {
            ImGui::TextDisabled("No AVDs found");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("AvdList", ImVec2(0, 0), ImGuiChildFlags_None);

        for (int i = 0; i < static_cast<int>(context.Catalog.Avds.size()); i++) {
            const auto &avd = context.Catalog.Avds[i];
            const bool isSelected = context.Catalog.SelectedAvd == i;
            const bool isRunning = context.Host.Manager.IsRunning(avd.Name);

            ImGui::PushID(i);
            const char *avdStatusText = isRunning ? "Running..." : "Ready";
            const ImVec4 avdStatusColor = isRunning ? HexColor("#33CC47") : HexColor("#66666B");
            if (SelectableItem(avd.DisplayName.c_str(), isSelected, avdStatusText, avdStatusColor)) {
                context.Catalog.SelectedAvd = i;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

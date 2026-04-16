//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//
#include "imgui.h"

#include "avd_list.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    void BuildAvdListWindow(Context &context) {
        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Available AVDs (Android Virtual Device)###AVDs", nullptr, panelFlags);

        if (PrimaryButton(Icons::Refresh)) RefreshAvds(context);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh the AVD list");

        ImGui::SameLine();


        if (context.SelectedAvd >= 0) {
            const auto &avd = context.Avds[context.SelectedAvd];
            const bool isRunning = context.Manager.IsRunning(avd.Name);
            const auto args = BuildArgs(avd.Name, GetDefaultAvdOptions(context));

            ImGui::SameLine();
            if (isRunning) {
                if (NegativeButton(IconWithLabel(Icons::Stop, "Stop").c_str())) {
                    context.Manager.Stop(avd.Name);
                }
            } else {
                if (PositiveButton(Icons::Play)) {
                    context.Manager.Launch(avd.Name, args);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run the selected AVD");
                ImGui::SameLine();
                if (WarningButton(IconWithLabel(Icons::Terminal, "Wipe & Run").c_str())) {
                    auto wipeArgs = args;
                    wipeArgs.emplace_back("-wipe-data");
                    context.Manager.Launch(avd.Name, wipeArgs);
                }
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
                ImGui::SameLine(0, 15.0f);
                if (PrimaryButton(Icons::Trash)) {
                    context.ShowDeleteDialog = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete currently selected AVD");
            }

            if (isRunning) {
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
            }

            ImGui::SameLine();
            if (PrimaryButton(Icons::Plus)) {
                context.CreateParams = {};
                context.SelectedSystemImage = 0;
                context.SelectedDevice = 0;
                context.SelectedGpuMode = 0;
                context.CreateDataReady = false;
                context.CreateDataLoading = true;
                context.ShowCreateDialog = true;

                context.CreateDataFuture = std::async(std::launch::async, [&context] {
                    auto images = ListSystemImages(context.Sdk);
                    auto devices = ListDeviceProfiles(context.Sdk);
                    context.SystemImages = std::move(images);
                    context.DeviceProfiles = std::move(devices);
                    context.CreateDataLoading = false;
                    context.CreateDataReady = true;
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new AVD");
        }

        ImGui::Separator();

        if (context.Avds.empty()) {
            ImGui::TextDisabled("No AVDs found");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("AvdList", ImVec2(0, 0), ImGuiChildFlags_None);

        for (int i = 0; i < static_cast<int>(context.Avds.size()); i++) {
            const auto &avd = context.Avds[i];
            const bool isSelected = context.SelectedAvd == i;
            const bool isRunning = context.Manager.IsRunning(avd.Name);

            ImGui::PushID(i);
            const char *avdStatusText = isRunning ? "Running..." : "Ready";
            const ImVec4 avdStatusColor = isRunning ? HexColor("#33CC47") : HexColor("#66666B");
            if (SelectableItem(avd.DisplayName.c_str(), isSelected, avdStatusText, avdStatusColor)) {
                context.SelectedAvd = i;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

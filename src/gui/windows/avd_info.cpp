//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//
#include "imgui.h"

#include "avd_info.h"
#include "../application.h"
#include "../widgets.h"

namespace CoreDeck {
    void BuildAvdInfoWindow(Context &context) {
        if (!context.UI.ShowDetailsPanel) return;

        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        if (context.Catalog.SelectedAvd < 0) {
            ImGui::Begin("Details###Details", nullptr, panelFlags);
            ImGui::TextDisabled("Select an AVD to view details");
            ImGui::End();
            return;
        }

        const auto &[
            Name,
            DisplayName,
            Device,
            ApiLevel,
            Abi,
            SdCard,
            RamSize,
            ScreenResolution,
            GpuMode,
            Arch,
            Path
        ] = context.Catalog.Avds[context.Catalog.SelectedAvd];
        const auto args = BuildArgs(Name, GetDefaultAvdOptions(context));

        std::string preview = context.Host.Sdk.EmulatorPath;
        for (const auto &arg: args) preview += " " + arg;

        ImGui::Begin(
            ("Details - " + DisplayName + "###Details").c_str(),
            nullptr,
            panelFlags
        );

        PropertyTextWrapped("AVD Path", Path.c_str());
        ImGui::Spacing();
        PropertyTextWrapped("Command", preview.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!Device.empty()) PropertyText("Device", Device.c_str(), false, true);
        if (!ApiLevel.empty()) PropertyText("API Level", ApiLevel.c_str(), false, true);
        if (!Abi.empty()) PropertyText("ABI", Abi.c_str(), false, true);
        if (!Arch.empty()) PropertyText("Arch", Arch.c_str(), false, true);
        if (!RamSize.empty()) PropertyText("RAM", (RamSize + " MB").c_str(), false, true);
        if (!ScreenResolution.empty()) PropertyText("Resolution", ScreenResolution.c_str(), false, true);
        if (!SdCard.empty()) PropertyText("Storage", SdCard.c_str(), false, true);
        if (!GpuMode.empty()) PropertyText("GPU Mode", GpuMode.c_str(), false, true);

        ImGui::End();
    }
}

//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//
#include <filesystem>

#include "imgui.h"

#include "avd_info.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/utilities.h"

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

        if (!Path.empty() && std::filesystem::exists(Path)) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto &diskCache = context.DiskUsage.PerAvdCache;
            auto it = diskCache.find(Name);
            if (it == diskCache.end()) {
                const std::uintmax_t size = GetDirectorySize(Path);
                diskCache[Name] = size;
                it = diskCache.find(Name);
            }

            const std::string sizeStr = FormatFileSize(it->second);
            PropertyText("Disk Usage", sizeStr.c_str(), false, true);

            ImGui::Spacing();

            const bool isRunning = context.Host.Manager.IsRunning(Name);
            if (isRunning) ImGui::BeginDisabled();
            if (WarningButton("Wipe User Data", !isRunning)) {
                context.UI.ShowWipeDataDialog = true;
            }
            if (isRunning) ImGui::EndDisabled();
            if (isRunning && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Stop the emulator before wiping data");
            }
        }

        if (!context.Jobs.AvdWipe.Busy.load() && context.Jobs.AvdWipe.Future.valid()) {
            context.Jobs.AvdWipe.Future.get();
            context.UI.ShowWipeDataDialog = false;
        }

        if (context.UI.ShowWipeDataDialog) {
            const bool isWiping = context.Jobs.AvdWipe.Busy.load();
            DialogData wipeDialog{
                .Id = "WipeUserData",
                .isOpen = context.UI.ShowWipeDataDialog,
                .title = "Wipe User Data",
                .message =
                "This will delete userdata, cache, SD card images, and snapshots for this AVD. This cannot be undone.\n\nContinue?",
                .confirmButtonTitle = "Wipe",
                .cancelButtonTitle = "Cancel",
                .busyButtonTitle = "Wiping...",
                .type = DialogType::Negative,
                .isBusy = isWiping,
            };
            if (const auto result = SimpleDialog(wipeDialog); result == DialogResult::Confirmed) {
                context.Jobs.AvdWipe.Busy = true;
                const std::string wipePath = Path;
                const std::string wipeName = Name;
                context.Jobs.AvdWipe.Future = std::async(std::launch::async, [&context, wipePath, wipeName] {
                    WipeAvdUserData(wipePath);
                    context.DiskUsage.PerAvdCache.erase(wipeName);
                    context.Jobs.AvdWipe.Busy = false;
                });
            }
        }

        ImGui::End();
    }
}

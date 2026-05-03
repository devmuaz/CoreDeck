//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//
#include <filesystem>
#include <sstream>

#include "imgui.h"

#include "avd_info.h"
#include "../application.h"
#include "../widgets.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    static std::string JoinAvdInfoList(const std::vector<std::string> &items) {
        std::stringstream stream;
        for (int i = 0; i < static_cast<int>(items.size()); i++) {
            if (i > 0) stream << ", ";
            stream << items[i];
        }
        return stream.str();
    }

    static const char *SystemImageKindLabel(const AvdInfo &avd) {
        if (avd.IsGooglePlayImage) return "Google Play";
        if (avd.IsGoogleApisImage) return "Google APIs";
        if (!avd.SystemImageTagDisplay.empty()) return avd.SystemImageTagDisplay.c_str();
        return "Default";
    }

    void BuildAvdInfoWindow(Context &context) {
        if (!context.UI.ShowDetailsPanel) return;

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        if (context.Catalog.SelectedAvd < 0) {
            ImGui::Begin("Details###Details", nullptr, flags);
            ImGui::TextDisabled("Select an AVD to view details");
            ImGui::End();
            return;
        }

        const auto &avd = context.Catalog.Avds[context.Catalog.SelectedAvd];
        const auto &Name = avd.Name;
        const auto &DisplayName = avd.DisplayName;
        const auto &Device = avd.Device;
        const auto &ApiLevel = avd.ApiLevel;
        const auto &Abi = avd.Abi;
        const auto &SdCard = avd.SdCard;
        const auto &RamSize = avd.RamSize;
        const auto &ScreenResolution = avd.ScreenResolution;
        const auto &GpuMode = avd.GpuMode;
        const auto &Arch = avd.Arch;
        const auto &Path = avd.Path;
        const auto args = BuildArgs(Name, GetDefaultAvdOptions(context));

        std::string preview = context.Host.Sdk.EmulatorPath;
        for (const auto &arg: args) preview += " " + arg;

        ImGui::Begin(("Details - " + DisplayName + "###Details").c_str(), nullptr, flags);

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
        if (!GpuMode.empty()) PropertyText("GPU Mode", GpuModeDisplayLabel(GpuMode), false, true);

        if (!avd.SystemImagePath.empty() ||
            !avd.SystemImageVariant.empty() ||
            !avd.SystemImageTagDisplay.empty() ||
            !avd.SystemImageTagDisplayNames.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            PropertyText("Type", SystemImageKindLabel(avd), false, true);
            PropertyText("16 KB Page Size", avd.Supports16KbPageSize ? "Supported" : "Not supported", false, true);
            if (!avd.SystemImageTagDisplayNames.empty()) {
                const std::string tags = JoinAvdInfoList(avd.SystemImageTagDisplayNames);
                PropertyTextWrapped("Tags", tags.c_str(), true);
            }
        }

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
            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5f;

            if (isRunning) ImGui::BeginDisabled();
            if (WarningButton("Wipe User Data", !isRunning, ImVec2(halfWidth, 0))) {
                context.UI.ShowWipeDataDialog = true;
            }
            if (isRunning) ImGui::EndDisabled();
            if (isRunning && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Stop the emulator before wiping data");
            }

            ImGui::SameLine();
            if (PrimaryButton("Storage Overview", true, ImVec2(halfWidth, 0))) {
                context.UI.ShowStorageDialog = true;
            }
        }

        if (!context.Jobs.AvdWipe.Busy.load() && context.Jobs.AvdWipe.Future.valid()) {
            context.Jobs.AvdWipe.Future.get();
            context.UI.ShowWipeDataDialog = false;
        }

        if (context.UI.ShowWipeDataDialog) {
            const bool isWiping = context.Jobs.AvdWipe.Busy.load();
            const DialogData wipeDialog{
                .Id = "WipeUserData",
                .isOpen = context.UI.ShowWipeDataDialog,
                .title = "Wipe User Data",
                .message = "This will delete userdata, cache, SD card images, and snapshots for this AVD. This cannot be undone.\n\nContinue?",
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

//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//
#include <filesystem>
#include <sstream>
#include <vector>

#include "imgui.h"

#include "avd_info.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/process_stats.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    namespace {
        std::string JoinAvdInfoList(const std::vector<std::string> &items) {
            std::stringstream stream;
            for (int i = 0; i < static_cast<int>(items.size()); i++) {
                if (i > 0) {
                    stream << ", ";
                }
                stream << items[i];
            }
            return stream.str();
        }

        const char *SystemImageKindLabel(const AvdInfo &avd) {
            if (avd.IsGooglePlayImage) {
                return "Google Play";
            }
            if (avd.IsGoogleApisImage) {
                return "Google APIs";
            }
            if (!avd.SystemImageTagDisplay.empty()) {
                return avd.SystemImageTagDisplay.c_str();
            }
            return "Default";
        }

        std::string FormatBytesPerSec(const std::uint64_t bytesPerSec) {
            return FormatFileSize(bytesPerSec) + "/s";
        }

        std::string FormatUptime(const std::chrono::seconds total) {
            const auto secs = total.count();
            const auto h = secs / 3600;
            const auto m = (secs % 3600) / 60;
            const auto s = secs % 60;

            char buf[64];
            if (h > 0) {
                (void) std::snprintf(buf, sizeof(buf), "%lldh %lldm %llds", static_cast<long long>(h), static_cast<long long>(m), static_cast<long long>(s));
            } else if (m > 0) {
                (void) std::snprintf(buf, sizeof(buf), "%lldm %llds", static_cast<long long>(m), static_cast<long long>(s));
            } else {
                (void) std::snprintf(buf, sizeof(buf), "%llds", static_cast<long long>(s));
            }
            return buf;
        }

        void DrawLiveResourceUsage(const Context &context, const std::string &avdName) {
            const bool isRunning = context.Host.Manager.IsRunning(avdName);
            const ProcessId pid = isRunning ? context.Host.Manager.GetPid(avdName) : 0;

            const auto &sampler = context.Host.Manager.Stats();
            ProcessSample latest{};
            std::chrono::seconds uptime{0};
            std::vector<float> cpuHist;
            std::vector<float> rssHist;
            if (pid != 0) {
                latest = sampler.Latest(pid);
                uptime = sampler.Uptime(pid);
                sampler.CopyCpuHistory(pid, cpuHist);
                sampler.CopyRssHistoryMb(pid, rssHist);
            } else {
                cpuHist.assign(PROCESS_STATS_HISTORY, 0.0F);
                rssHist.assign(PROCESS_STATS_HISTORY, 0.0F);
            }

            ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), "Live Resource Usage");
            ImGui::Spacing();

            if (!isRunning) {
                ImGui::BeginDisabled();
            }

            const float chartWidth = ImGui::GetContentRegionAvail().x;
            const float chartHeight = 70.0F;

            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "CPU Usage");
            {
                StyleColor sc;
                sc.Push(ImGuiCol_PlotLines, HexColor(Colors::POSITIVE));
                sc.Push(ImGuiCol_FrameBg, HexColor(Colors::SURFACE1));
                ImGui::PlotLines("##CPUChart", cpuHist.data(), static_cast<int>(cpuHist.size()), 0, nullptr, 0.0F, 100.0F, ImVec2(chartWidth, chartHeight));
            }

            char cpuValue[32];
            if (isRunning && latest.Valid) {
                (void) std::snprintf(cpuValue, sizeof(cpuValue), "%.1f%%", latest.CpuPercent);
            } else {
                (void) std::snprintf(cpuValue, sizeof(cpuValue), "—");
            }
            PropertyText("Current CPU", cpuValue, false, true);

            ImGui::Spacing();

            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "Memory");
            {
                StyleColor sc;
                sc.Push(ImGuiCol_PlotLines, HexColor(Colors::ACCENT_INFO));
                sc.Push(ImGuiCol_FrameBg, HexColor(Colors::SURFACE1));
                ImGui::PlotLines("##RAMChart", rssHist.data(), static_cast<int>(rssHist.size()), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(chartWidth, chartHeight));
            }

            char ramValue[32];
            if (isRunning && latest.Valid) {
                const std::string rss = FormatFileSize(latest.RssBytes);
                (void) std::snprintf(ramValue, sizeof(ramValue), "%s", rss.c_str());
            } else {
                (void) std::snprintf(ramValue, sizeof(ramValue), "—");
            }
            PropertyText("Current Memory", ramValue, false, true);

            ImGui::Spacing();

            const std::string readRate =
                (isRunning && latest.Valid)
                    ? FormatBytesPerSec(latest.DiskReadBytesPerSec)
                    : std::string("—");

            const std::string writeRate =
                (isRunning && latest.Valid)
                    ? FormatBytesPerSec(latest.DiskWriteBytesPerSec)
                    : std::string("—");

            PropertyText("Disk Read Rate", readRate.c_str(), false, true);
            PropertyText("Disk Write Rate", writeRate.c_str(), false, true);

            const std::string uptimeStr = isRunning ? FormatUptime(uptime) : std::string("—");
            PropertyText("Uptime", uptimeStr.c_str(), false, true);

            if (!isRunning) {
                ImGui::EndDisabled();
            }
        }
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildAvdInfoWindow(Context &context) {

        if (!context.UI.ShowDetailsPanel) {
            return;
        }

        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        if (context.Catalog.SelectedAvd < 0) {
            ImGui::Begin("Details###Details", nullptr, FLAGS);
            ImGui::TextDisabled("Select an AVD to view details");
            ImGui::End();
            return;
        }

        const auto &avd = context.Catalog.Avds[context.Catalog.SelectedAvd];
        const auto &path = avd.Path;
        const auto &name = avd.Name;
        const auto &displayName = avd.DisplayName;
        const auto &device = avd.Device;
        const auto &apiLevel = avd.ApiLevel;
        const auto &abi = avd.Abi;
        const auto &arch = avd.Arch;
        const auto &ramSize = avd.RamSize;
        const auto &screenResolution = avd.ScreenResolution;
        const auto &gpuMode = avd.GpuMode;
        const auto &skinName = avd.SkinName;
        const auto &sdCard = avd.SdCard;
        ImGui::Begin(("Details - " + displayName + "###Details").c_str(), nullptr, FLAGS);

        DrawLiveResourceUsage(context, name);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!device.empty()) {
            PropertyText("Device", device.c_str(), false, true);
        }
        if (!apiLevel.empty()) {
            PropertyText("API Level", apiLevel.c_str(), false, true);
        }
        if (!abi.empty()) {
            PropertyText("ABI", abi.c_str(), false, true);
        }
        if (!arch.empty()) {
            PropertyText("Arch", arch.c_str(), false, true);
        }
        if (!ramSize.empty()) {
            PropertyText("RAM", (ramSize + " MB").c_str(), false, true);
        }
        if (!screenResolution.empty()) {
            PropertyText("Resolution", screenResolution.c_str(), false, true);
        }
        if (!sdCard.empty()) {
            PropertyText("Storage", sdCard.c_str(), false, true);
        }
        if (!gpuMode.empty()) {
            PropertyText("GPU Mode", GpuModeDisplayLabel(gpuMode), false, true);
        }
        PropertyText("Skin", skinName.empty() ? "None" : skinName.c_str(), false, true);

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
                ImGui::Spacing();
                ImGui::TextDisabled("Tags");
                ImGui::TextWrapped("%s", tags.c_str());
            }
        }

        if (!path.empty() && std::filesystem::exists(path)) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto &diskCache = context.DiskUsage.PerAvdCache;
            auto it = diskCache.find(name);
            if (it == diskCache.end()) {
                const std::uintmax_t size = GetDirectorySize(path);
                diskCache[name] = size;
                it = diskCache.find(name);
            }

            const std::string sizeStr = FormatFileSize(it->second);
            PropertyText("Disk Usage", sizeStr.c_str(), false, true);

            ImGui::Spacing();

            const bool isRunning = context.Host.Manager.IsRunning(name);
            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5F;

            if (isRunning) {
                ImGui::BeginDisabled();
            }
            if (WarningButton("Wipe User Data", !isRunning, ImVec2(halfWidth, 0))) {
                context.UI.ShowWipeDataDialog = true;
            }
            if (isRunning) {
                ImGui::EndDisabled();
            }
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
                .IsOpen = context.UI.ShowWipeDataDialog,
                .Title = "Wipe User Data",
                .Message = "This will delete userdata, cache, SD card images, and snapshots for this AVD. This cannot be undone.\n\nContinue?",
                .ConfirmButtonTitle = "Wipe",
                .CancelButtonTitle = "Cancel",
                .BusyButtonTitle = "Wiping...",
                .Type = DialogType::Negative,
                .IsBusy = isWiping,
            };
            if (const auto result = SimpleDialog(wipeDialog); result == DialogResult::Confirmed) {
                context.Jobs.AvdWipe.Busy = true;
                const std::string wipePath = path;
                const std::string wipeName = name;
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

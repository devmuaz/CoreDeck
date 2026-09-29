//
// Created by AbdulMuaz Aqeel on 19/04/2026.
//

#include <chrono>
#include <filesystem>
#include <future>

#include "imgui.h"

#include "storage.h"
#include "../widgets.h"
#include "../theme.h"
#include "gui/context.h"
#include "../../core/utilities.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        StorageScanResult ScanStorageUsage(const std::vector<AvdInfo> &avds, const std::string &sdkPath) {
            StorageScanResult result;

            for (const auto &avd: avds) {
                if (avd.Path.empty()) {
                    continue;
                }
                if (std::filesystem::exists(avd.Path)) {
                    result.TotalAvdSize += GetDirectorySize(avd.Path);
                }
            }

            if (!sdkPath.empty()) {
                const auto sysImgRoot = std::filesystem::path(sdkPath) / "system-images";
                if (std::filesystem::exists(sysImgRoot)) {
                    result.SystemImagesSize = GetDirectorySize(sysImgRoot.string());
                }
            }

            return result;
        }

        void StartStorageScan(Context &context) {
            auto &disk = context.DiskUsage;
            disk.Loading = true;
            disk.Ready = false;

            const auto avds = context.Catalog.Avds;
            const std::string sdkPath = context.Host.Sdk.SdkPath;
            disk.Future = std::async(std::launch::async, [avds, sdkPath] {
                return ScanStorageUsage(avds, sdkPath);
            });
        }

        void DrawStorageSummaryCard(const char *title, const std::string &value, const char *accentColor, const float width) {
            StyleColor sc;
            StyleVar sv;
            sc.Push(ImGuiCol_ChildBg, HexColor(Colors::SURFACE1));
            sc.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
            sv.Push(ImGuiStyleVar_ChildRounding, 8.0F);
            sv.Push(ImGuiStyleVar_ChildBorderSize, 1.0F);
            sv.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F, 12.0F));

            ImGui::BeginChild(title, ImVec2(width, Eh(3.7F)), 1, ImGuiWindowFlags_NoScrollbar);
            ImGui::TextDisabled("%s", title);
            ImGui::Spacing();
            ImGui::TextColored(HexColor(accentColor), "%s", value.c_str());
            ImGui::EndChild();
        }

        void DrawStorageBreakdownBar(const std::uintmax_t avdSize, const std::uintmax_t systemImageSize) {
            const std::uintmax_t total = avdSize + systemImageSize;
            const float width = ImGui::GetContentRegionAvail().x;
            constexpr float HEIGHT = 14.0F;
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 end(pos.x + width, pos.y + HEIGHT);
            auto *drawList = ImGui::GetWindowDrawList();

            const bool light = IsLightColorScheme();
            const ImU32 track = ImGui::ColorConvertFloat4ToU32(HexColor(light ? Colors::SURFACE3 : Colors::SURFACE2));
            drawList->AddRectFilled(pos, end, track, 999.0F);
            if (total > 0) {
                const float avdWidth = width * (static_cast<float>(avdSize) / static_cast<float>(total));
                const bool hasBoth = avdSize > 0 && systemImageSize > 0;
                if (avdSize > 0) {
                    const ImDrawFlags avdCorners = hasBoth ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersAll;
                    drawList->AddRectFilled(pos, ImVec2(pos.x + avdWidth, end.y), ImGui::ColorConvertFloat4ToU32(HexColor(Colors::STORAGE_AVD_FILL)), 999.0F, avdCorners);
                }
                if (systemImageSize > 0) {
                    const ImDrawFlags sysCorners = hasBoth ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersAll;
                    drawList->AddRectFilled(ImVec2(pos.x + avdWidth, pos.y), end, ImGui::ColorConvertFloat4ToU32(HexColor(Colors::STORAGE_SYSTEM_IMAGE_FILL)), 999.0F, sysCorners);
                }
            }
            if (light) {
                drawList->AddRect(pos, end, ImGui::ColorConvertFloat4ToU32(HexColor(Colors::BORDER)), 999.0F, 0, 1.0F);
            }

            ImGui::Dummy(ImVec2(width, HEIGHT));
        }
    }

    void BuildStorageWindow(Context &context) {
        const std::string title = TrWindow("Storage Overview", "StorageDialog");
        if (!context.UI.ShowStorageDialog && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        if (BeginCenteredModal(title.c_str(), &context.UI.ShowStorageDialog, EmV(80.0F, 0.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            auto &disk = context.DiskUsage;

            if (!disk.Ready && !disk.Loading.load() && !disk.Future.valid()) {
                StartStorageScan(context);
            }

            if (disk.Loading.load() && disk.Future.valid() &&
                disk.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                disk.LastScan = disk.Future.get();
                disk.Ready = true;
                disk.Loading = false;
            }

            const bool isLoading = disk.Loading.load();
            const auto &[totalAvdSize, systemImagesSize] = disk.LastScan;
            const std::uintmax_t grandTotal = totalAvdSize + systemImagesSize;

            ImGui::Text("%s", Tr("Statistics"));
            ImGui::TextDisabled("%s", isLoading ? Tr("Calculating...") : Tr("Calculated from local SDK and AVD folders"));

            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float cardWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0F)) / 3.0F;
            DrawStorageSummaryCard(Tr("Total Storage"), isLoading && !disk.Ready ? std::string(Tr("Calculating...")) : FormatFileSize(grandTotal), Colors::TEXT_PRIMARY, cardWidth);
            ImGui::SameLine();
            DrawStorageSummaryCard(Tr("AVDs"), isLoading && !disk.Ready ? std::string(Tr("Calculating...")) : FormatFileSize(totalAvdSize), Colors::STORAGE_AVD, cardWidth);
            ImGui::SameLine();
            DrawStorageSummaryCard(Tr("System Images"), isLoading && !disk.Ready ? std::string(Tr("Calculating...")) : FormatFileSize(systemImagesSize), Colors::STORAGE_SYSTEM_IMAGE, cardWidth);

            ImGui::Spacing();
            ImGui::TextDisabled("%s", Tr("Breakdown"));
            DrawStorageBreakdownBar(totalAvdSize, systemImagesSize);
            ImGui::Spacing();
            ImGui::TextColored(HexColor(Colors::STORAGE_AVD), "%s", Tr("AVDs"));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", FormatFileSize(totalAvdSize).c_str());
            ImGui::SameLine();
            ImGui::TextColored(HexColor(Colors::STORAGE_SYSTEM_IMAGE), "%s", Tr("System Images"));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", FormatFileSize(systemImagesSize).c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);
            if (PositiveButton(isLoading ? Tr("Refreshing...") : Tr("Refresh"), !isLoading, ImVec2(halfWidth, 0))) {
                StartStorageScan(context);
            }
            ImGui::SameLine();
            if (PrimaryButton(Tr("Close"), !isLoading, ImVec2(halfWidth, 0))) {
                context.UI.ShowStorageDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

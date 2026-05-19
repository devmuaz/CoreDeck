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

            drawList->AddRectFilled(pos, end, ImGui::ColorConvertFloat4ToU32(HexColor(Colors::SURFACE2)), 999.0F);
            if (total > 0) {
                const float avdWidth = width * (static_cast<float>(avdSize) / static_cast<float>(total));
                const bool hasBoth = avdSize > 0 && systemImageSize > 0;
                if (avdSize > 0) {
                    const ImDrawFlags avdCorners = hasBoth ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersAll;
                    drawList->AddRectFilled(pos, ImVec2(pos.x + avdWidth, end.y), ImGui::ColorConvertFloat4ToU32(HexColor(Colors::STORAGE_AVD)), 999.0F, avdCorners);
                }
                if (systemImageSize > 0) {
                    const ImDrawFlags sysCorners = hasBoth ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersAll;
                    drawList->AddRectFilled(ImVec2(pos.x + avdWidth, pos.y), end, ImGui::ColorConvertFloat4ToU32(HexColor(Colors::STORAGE_SYSTEM_IMAGE)), 999.0F, sysCorners);
                }
            }

            ImGui::Dummy(ImVec2(width, HEIGHT));
        }
    }

    void BuildStorageWindow(Context &context) {
        if (context.UI.ShowStorageDialog && !ImGui::IsPopupOpen("Storage Overview###StorageDialog")) {
            ImGui::OpenPopup("Storage Overview###StorageDialog");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(EmV(80.0F, 0.0F), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Storage Overview###StorageDialog", &context.UI.ShowStorageDialog, WINDOW_AUTO_RESIZE_FLAGS)) {
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
            const auto &[TotalAvdSize, SystemImagesSize] = disk.LastScan;
            const std::uintmax_t grandTotal = TotalAvdSize + SystemImagesSize;

            ImGui::Text("Statistics");
            ImGui::TextDisabled("%s", isLoading ? "Calculating..." : "Calculated from local SDK and AVD folders");

            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float cardWidth = (ImGui::GetContentRegionAvail().x - (spacing * 2.0F)) / 3.0F;
            DrawStorageSummaryCard("Total Storage", isLoading && !disk.Ready ? "Calculating..." : FormatFileSize(grandTotal), Colors::TEXT_PRIMARY, cardWidth);
            ImGui::SameLine();
            DrawStorageSummaryCard("AVDs", isLoading && !disk.Ready ? "Calculating..." : FormatFileSize(TotalAvdSize), Colors::STORAGE_AVD, cardWidth);
            ImGui::SameLine();
            DrawStorageSummaryCard("System Images", isLoading && !disk.Ready ? "Calculating..." : FormatFileSize(SystemImagesSize), Colors::STORAGE_SYSTEM_IMAGE, cardWidth);

            ImGui::Spacing();
            ImGui::TextDisabled("Breakdown");
            DrawStorageBreakdownBar(TotalAvdSize, SystemImagesSize);
            ImGui::Spacing();
            ImGui::TextColored(HexColor(Colors::STORAGE_AVD), "AVDs");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", FormatFileSize(TotalAvdSize).c_str());
            ImGui::SameLine();
            ImGui::TextColored(HexColor(Colors::STORAGE_SYSTEM_IMAGE), "System Images");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", FormatFileSize(SystemImagesSize).c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5F;
            if (PositiveButton(isLoading ? "Refreshing..." : "Refresh", !isLoading, ImVec2(halfWidth, 0))) {
                StartStorageScan(context);
            }
            ImGui::SameLine();
            if (PrimaryButton("Close", !isLoading, ImVec2(halfWidth, 0))) {
                context.UI.ShowStorageDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

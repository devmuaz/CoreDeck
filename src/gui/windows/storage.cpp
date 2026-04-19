//
// Created by AbdulMuaz Aqeel on 19/04/2026.
//

#include <algorithm>
#include <filesystem>

#include "imgui.h"

#include "storage.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/utilities.h"
#include "../../core/paths.h"

namespace CoreDeck {
    void BuildStorageWindow(Context &context) {
        if (context.UI.ShowStorageDialog && !ImGui::IsPopupOpen("Storage Overview###StorageDialog")) {
            ImGui::OpenPopup("Storage Overview###StorageDialog");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal("Storage Overview###StorageDialog", &context.UI.ShowStorageDialog, flags)) {
            auto &cache = context.DiskUsage.PerAvdCache;

            // --- Summary ---
            std::uintmax_t totalAvdSize = 0;
            for (const auto &avd : context.Catalog.Avds) {
                if (avd.Path.empty() || !std::filesystem::exists(avd.Path)) continue;
                auto it = cache.find(avd.Name);
                if (it == cache.end()) {
                    const std::uintmax_t size = GetDirectorySize(avd.Path);
                    cache[avd.Name] = size;
                    it = cache.find(avd.Name);
                }
                totalAvdSize += it->second;
            }

            // System images directory size (cached)
            if (!context.DiskUsage.SystemImagesSizeCached && !context.Host.Sdk.SdkPath.empty()) {
                const auto sysImgDir = std::filesystem::path(context.Host.Sdk.SdkPath) / "system-images";
                if (std::filesystem::exists(sysImgDir)) {
                    context.DiskUsage.SystemImagesSize = GetDirectorySize(sysImgDir.string());
                }
                context.DiskUsage.SystemImagesSizeCached = true;
            }
            const std::uintmax_t sysImgTotal = context.DiskUsage.SystemImagesSize;

            const std::string avdSizeStr = FormatFileSize(totalAvdSize);
            const std::string imgSizeStr = FormatFileSize(sysImgTotal);
            const std::uintmax_t grandTotal = totalAvdSize + sysImgTotal;
            const std::string totalStr = FormatFileSize(grandTotal);

            const float contentMax = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();

            ImGui::Text("Total AVD Size");
            ImGui::SameLine(contentMax - ImGui::CalcTextSize(avdSizeStr.c_str()).x);
            ImGui::Text("%s", avdSizeStr.c_str());

            ImGui::Text("System Images");
            ImGui::SameLine(contentMax - ImGui::CalcTextSize(imgSizeStr.c_str()).x);
            ImGui::Text("%s", imgSizeStr.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("Total SDK Storage");
            ImGui::SameLine(contentMax - ImGui::CalcTextSize(totalStr.c_str()).x);
            ImGui::Text("%s", totalStr.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- Per-AVD breakdown ---
            if (CollapsingHeader("AVDs")) {
                if (context.Catalog.Avds.empty()) {
                    ImGui::TextDisabled("No AVDs found.");
                } else {
                    // Sort AVDs by size descending for the overview
                    struct AvdSizeEntry {
                        std::string Name;
                        std::string DisplayName;
                        std::uintmax_t Size;
                    };
                    std::vector<AvdSizeEntry> entries;
                    entries.reserve(context.Catalog.Avds.size());
                    for (const auto &avd : context.Catalog.Avds) {
                        std::uintmax_t size = 0;
                        if (auto it = cache.find(avd.Name); it != cache.end()) {
                            size = it->second;
                        }
                        entries.push_back({avd.Name, avd.DisplayName, size});
                    }
                    std::ranges::sort(entries, [](const auto &a, const auto &b) {
                        return a.Size > b.Size;
                    });

                    for (const auto &[name, displayName, size] : entries) {
                        const std::string sizeStr = FormatFileSize(size);
                        const std::string label = displayName.empty() ? name : displayName;
                        ImGui::Text("%s", label.c_str());
                        ImGui::SameLine(contentMax - ImGui::CalcTextSize(sizeStr.c_str()).x);
                        ImGui::TextDisabled("%s", sizeStr.c_str());
                    }
                }
            }

            ImGui::Spacing();

            // --- Per-system-image breakdown ---
            if (CollapsingHeader("Installed System Images")) {
                if (!context.Host.Sdk.SdkPath.empty()) {
                    // Build the cache on first access
                    if (!context.DiskUsage.SystemImageEntriesCached) {
                        context.DiskUsage.SystemImageEntries.clear();
                        const auto sysImgRoot = std::filesystem::path(context.Host.Sdk.SdkPath) / "system-images";
                        if (std::filesystem::exists(sysImgRoot)) {
                            std::error_code ec;
                            // system-images/<api>/<variant>/<abi>
                            for (const auto &apiDir : std::filesystem::directory_iterator(sysImgRoot, ec)) {
                                if (!apiDir.is_directory()) continue;
                                for (const auto &variantDir : std::filesystem::directory_iterator(apiDir.path(), ec)) {
                                    if (!variantDir.is_directory()) continue;
                                    for (const auto &abiDir : std::filesystem::directory_iterator(variantDir.path(), ec)) {
                                        if (!abiDir.is_directory()) continue;
                                        const std::string label =
                                            apiDir.path().filename().string() + " / " +
                                            variantDir.path().filename().string() + " / " +
                                            abiDir.path().filename().string();
                                        const std::uintmax_t size = GetDirectorySize(abiDir.path().string());
                                        context.DiskUsage.SystemImageEntries.push_back({label, size});
                                    }
                                }
                            }
                            std::ranges::sort(context.DiskUsage.SystemImageEntries, [](const auto &a, const auto &b) {
                                return a.Size > b.Size;
                            });
                        }
                        context.DiskUsage.SystemImageEntriesCached = true;
                    }

                    if (context.DiskUsage.SystemImageEntries.empty()) {
                        ImGui::TextDisabled("No system images found.");
                    } else {
                        for (const auto &[name, size] : context.DiskUsage.SystemImageEntries) {
                            const std::string sizeStr = FormatFileSize(size);
                            ImGui::Text("%s", name.c_str());
                            ImGui::SameLine(contentMax - ImGui::CalcTextSize(sizeStr.c_str()).x);
                            ImGui::TextDisabled("%s", sizeStr.c_str());
                        }
                    }
                } else {
                    ImGui::TextDisabled("SDK path not configured.");
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- Close button ---
            constexpr float closeW = 100.0f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - closeW + ImGui::GetCursorPosX());
            if (PrimaryButton("Close", true, ImVec2(closeW, 0))) {
                context.UI.ShowStorageDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include <chrono>
#include "imgui.h"

#include "create_avd.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    static int DigitsOnlyFilter(ImGuiInputTextCallbackData *data) {
        return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
    }

    static int AvdNameFilter(ImGuiInputTextCallbackData *data) {
        const ImWchar c = data->EventChar;
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        return ok ? 0 : 1;
    }

    static bool AvdNameExists(const std::vector<std::string> &names, const std::string &candidate) {
        if (candidate.empty()) return false;
        auto lower = [](std::string s) {
            std::ranges::transform(s, s.begin(), [](const unsigned char ch) { return std::tolower(ch); });
            return s;
        };
        const std::string needle = lower(candidate);
        return std::ranges::any_of(names, [&](const std::string &n) { return lower(n) == needle; });
    }

    void BuildCreateAvdWindow(Context &context) {
        if (context.UI.ShowCreateAvdDialog && !ImGui::IsPopupOpen("Create New AVD###CreateAvdDialog")) {
            ImGui::OpenPopup("Create New AVD###CreateAvdDialog");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal("Create New AVD###CreateAvdDialog", &context.UI.ShowCreateAvdDialog, flags)) {
            auto &removal = context.AvdCreationWork.SystemImageRemoval;
            if (removal.Future.valid()) {
                if (removal.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const bool ok = removal.Future.get();
                    if (ok) {
                        context.AvdCreationWork.SystemImages = ListSystemImages(context.Host.Sdk);
                        if (context.AvdCreationWork.SystemImages.empty()) {
                            context.AvdCreationWork.SelectedSystemImage = 0;
                        } else {
                            int &sel = context.AvdCreationWork.SelectedSystemImage;
                            sel = std::clamp(sel, 0, static_cast<int>(context.AvdCreationWork.SystemImages.size()) - 1);
                        }
                    }
                }
            }

            const bool isLoading = context.AvdCreationWork.Prefetch.Loading.load();
            const bool isCreating = context.Jobs.AvdCreation.Busy.load();
            const bool formDisabled = isLoading || isCreating;
            const bool systemImageRemovalBusy = removal.Busy.load() || removal.Future.valid();

            if (formDisabled) ImGui::BeginDisabled();

            const float nameRowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float nameColWidth = (ImGui::GetContentRegionAvail().x - nameRowSpacing) * 0.5f;
            const float nameCol2X = ImGui::GetCursorPosX() + nameColWidth + nameRowSpacing;

            ImGui::Text("AVD Name");
            ImGui::SameLine();
            ImGui::SetCursorPosX(nameCol2X);
            ImGui::Text("Display Name");

            char nameBuffer[128];
            strncpy(nameBuffer, context.AvdCreationWork.CreationData.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(nameColWidth);
            if (ImGui::InputTextWithHint("##AvdName", "e.g. MyPixel7", nameBuffer, sizeof(nameBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, AvdNameFilter)) {
                context.AvdCreationWork.CreationData.Name = nameBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(nameCol2X);

            char displayBuffer[128];
            strncpy(displayBuffer, context.AvdCreationWork.CreationData.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(nameColWidth);
            if (ImGui::InputTextWithHint("##DisplayName", "e.g. My Pixel 7", displayBuffer, sizeof(displayBuffer))) {
                context.AvdCreationWork.CreationData.DisplayName = displayBuffer;
            }

            if (AvdNameExists(context.Catalog.AvdNames, context.AvdCreationWork.CreationData.Name)) {
                ImGui::TextColored(
                    HexColor("#E64D40"),
                    "An AVD named \"%s\" already exists.",
                    context.AvdCreationWork.CreationData.Name.c_str()
                );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("System Image");
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.SystemImages.empty()) {
                ImGui::TextColored(
                    HexColor("#E64D40"),
                    "No system images installed."
                );
            } else if (!context.AvdCreationWork.SystemImages.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##SystemImage",
                                      context.AvdCreationWork.SystemImages[context.AvdCreationWork.SelectedSystemImage].
                                      DisplayName.c_str())) {
                    for (int i = 0; i < static_cast<int>(context.AvdCreationWork.SystemImages.size()); i++) {
                        const bool isSelected = context.AvdCreationWork.SelectedSystemImage == i;
                        if (ImGui::Selectable(context.AvdCreationWork.SystemImages[i].DisplayName.c_str(),
                                              isSelected)) {
                            context.AvdCreationWork.SelectedSystemImage = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::BeginCombo("##SystemImage", "Loading...");
            }

            if (!context.Host.Sdk.SdkManagerPath.empty()) {
                if (PrimaryButton("Install New Image...", !formDisabled)) {
                    context.ImageInstallationWork.SelectedImage = 0;
                    context.ImageInstallationWork.Progress.reset();
                    context.ImageInstallationWork.Prefetch.Ready = false;
                    context.ImageInstallationWork.Prefetch.Loading = true;
                    context.UI.ShowInstallImageDialog = true;

                    context.ImageInstallationWork.Prefetch.Future = std::async(std::launch::async, [&context] {
                        auto localImages = ListSystemImages(context.Host.Sdk);
                        auto remoteImages = ListRemoteSystemImages(context.Host.Sdk, localImages);
                        context.ImageInstallationWork.RemoteImages = std::move(remoteImages);
                        context.ImageInstallationWork.Prefetch.Loading = false;
                        context.ImageInstallationWork.Prefetch.Ready = true;
                    });

                    context.UI.ReopenCreateAvdOnInstallClose = true;
                    context.UI.ShowCreateAvdDialog = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Download and install a new system image from the SDK");

                ImGui::SameLine();
                const bool canRemove =
                        !formDisabled &&
                        context.AvdCreationWork.Prefetch.Ready &&
                        !context.AvdCreationWork.SystemImages.empty() &&
                        context.AvdCreationWork.SelectedSystemImage >= 0 &&
                        context.AvdCreationWork.SelectedSystemImage < static_cast<int>(context.AvdCreationWork.SystemImages.size());
                if (systemImageRemovalBusy) {
                    ImGui::BeginDisabled();
                    NegativeButton("Removing...", false, ImVec2(0, 0));
                    ImGui::EndDisabled();
                } else {
                    if (NegativeButton("Remove Image...", canRemove)) {
                        const std::string pkg =
                                context.AvdCreationWork.SystemImages[context.AvdCreationWork.SelectedSystemImage].PackagePath;
                        removal.Busy = true;
                        removal.Future = std::async(std::launch::async, [&context, pkg]() {
                            try {
                                const bool ok = UninstallSystemImage(context.Host.Sdk, pkg);
                                context.AvdCreationWork.SystemImageRemoval.Busy = false;
                                return ok;
                            } catch (...) {
                                context.AvdCreationWork.SystemImageRemoval.Busy = false;
                                return false;
                            }
                        });
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Uninstall the selected system image via SDK manager");
                    }
                }
            }

            ImGui::Spacing();

            ImGui::Text("Device");
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.DeviceProfiles.empty()) {
                ImGui::TextColored(HexColor("#E64D40"), "No device profiles found.");
            } else if (!context.AvdCreationWork.DeviceProfiles.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo(
                    "##device",
                    context.AvdCreationWork.DeviceProfiles[context.AvdCreationWork.SelectedDevice].Name.c_str())) {
                    for (int i = 0; i < static_cast<int>(context.AvdCreationWork.DeviceProfiles.size()); i++) {
                        const bool isSelected = context.AvdCreationWork.SelectedDevice == i;
                        if (ImGui::Selectable(context.AvdCreationWork.DeviceProfiles[i].Name.c_str(), isSelected)) {
                            context.AvdCreationWork.SelectedDevice = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::BeginCombo("##device", "Loading...");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float colWidth = (ImGui::GetContentRegionAvail().x - rowSpacing) * 0.5f;
            const float col2X = ImGui::GetCursorPosX() + colWidth + rowSpacing;

            ImGui::Text("RAM (MB)");
            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);
            ImGui::Text("SD Card Size");

            char ramBuffer[32];
            strncpy(ramBuffer, context.AvdCreationWork.CreationData.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##ram", "e.g. 2048 (MB)", ramBuffer, sizeof(ramBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.RamSize = ramBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);

            char sdBuffer[32];
            strncpy(sdBuffer, context.AvdCreationWork.CreationData.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##sdcard", "e.g. 512 (MB)", sdBuffer, sizeof(sdBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("GPU Mode");
            static const char *gpuModes[] = {"auto", "host", "swiftshader_indirect", "guest"};
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##gpu", gpuModes[context.AvdCreationWork.SelectedGpuMode])) {
                for (int i = 0; i < 4; i++) {
                    const bool isSelected = context.AvdCreationWork.SelectedGpuMode == i;
                    if (ImGui::Selectable(gpuModes[i], isSelected)) {
                        context.AvdCreationWork.SelectedGpuMode = i;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (formDisabled) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            const bool canCreate = !context.AvdCreationWork.CreationData.Name.empty()
                                   && !context.AvdCreationWork.SystemImages.empty()
                                   && !context.AvdCreationWork.DeviceProfiles.empty()
                                   && !formDisabled;

            if (isCreating) {
                ImGui::BeginDisabled();
                PositiveButton("Creating...", false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                if (PositiveButton("Create", canCreate, ImVec2(halfWidth, 0))) {
                    const auto &SystemImagePackagePath = context.AvdCreationWork.SystemImages[
                        context.AvdCreationWork.SelectedSystemImage
                    ].PackagePath;

                    const auto &DeviceId = context.AvdCreationWork.DeviceProfiles[
                        context.AvdCreationWork.SelectedDevice
                    ].Id;

                    context.AvdCreationWork.CreationData.SystemImagePackagePath = SystemImagePackagePath;
                    context.AvdCreationWork.CreationData.DeviceId = DeviceId;
                    context.AvdCreationWork.CreationData.GpuMode = gpuModes[context.AvdCreationWork.SelectedGpuMode];
                    if (!context.AvdCreationWork.CreationData.SdCardSize.empty()) {
                        context.AvdCreationWork.CreationData.SdCardSize += "M";
                    }

                    context.Jobs.AvdCreation.Busy = true;
                    context.Jobs.AvdCreation.Future = std::async(std::launch::async, [&context] {
                        CreateAvd(context.Host.Sdk, context.AvdCreationWork.CreationData);
                        context.Jobs.AvdCreation.Busy = false;
                    });
                }
            }
            ImGui::SameLine();
            if (PrimaryButton("Cancel", !isCreating, ImVec2(halfWidth, 0))) {
                context.UI.ShowCreateAvdDialog = false;
                ImGui::CloseCurrentPopup();
            }

            if (!context.Jobs.AvdCreation.Busy && context.Jobs.AvdCreation.Future.valid()) {
                context.Jobs.AvdCreation.Future.get();
                context.UI.ShowCreateAvdDialog = false;
                ImGui::CloseCurrentPopup();
                RefreshAvds(context);
            }

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
}

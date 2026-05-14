//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include "imgui.h"

#include "create_avd.h"
#include "device_profile.h"
#include "install_image.h"
#include "skin.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    namespace {
        void OpenSystemImagePicker(Context &context) {
            context.ImageInstallationWork.SelectedImage = -1;
            context.ImageInstallationWork.SelectedCategory = ImageCategory::PhoneTablet;
            context.ImageInstallationWork.SearchFilter[0] = '\0';
            context.ImageInstallationWork.Progress.reset();
            context.ImageInstallationWork.Prefetch.Ready = false;
            context.ImageInstallationWork.Prefetch.Loading = true;
            context.UI.ShowInstallImageDialog = true;

            context.ImageInstallationWork.Prefetch.Future = std::async(std::launch::async, [&context] {
                const auto localImages = ListSystemImages(context.Host.Sdk);
                auto remoteImages = ListRemoteSystemImages(context.Host.Sdk, localImages);
                context.AvdCreationWork.SystemImages = localImages;
                context.ImageInstallationWork.RemoteImages = std::move(remoteImages);
                context.ImageInstallationWork.Prefetch.Loading = false;
                context.ImageInstallationWork.Prefetch.Ready = true;
            });
        }


        int DigitsOnlyFilter(ImGuiInputTextCallbackData *data) {
            return data->EventChar >= '0' && data->EventChar <= '9' ? 0 : 1;
        }


        int AvdNameFilter(ImGuiInputTextCallbackData *data) {
            const ImWchar c = data->EventChar;
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            return ok ? 0 : 1;
        }

        bool AvdNameExists(const std::vector<std::string> &names, const std::string &candidate) {
            if (candidate.empty()) {
                return false;
            }
            const std::string needle = LowerCopy(candidate);
            return std::ranges::any_of(names, [&](const std::string &n) { return LowerCopy(n) == needle; });
        }
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildCreateAvdWindow(Context &context) {
        if (context.UI.ShowCreateAvdDialog && !ImGui::IsPopupOpen("Create New AVD###CreateAvdDialog")) {
            ImGui::OpenPopup("Create New AVD###CreateAvdDialog");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Create New AVD###CreateAvdDialog", &context.UI.ShowCreateAvdDialog, WINDOW_AUTO_RESIZE_FLAGS)) {
            const bool isLoading = context.AvdCreationWork.Prefetch.Loading.load();
            const bool isCreating = context.Jobs.AvdCreation.Busy.load();
            const bool formDisabled = isLoading || isCreating;

            if (formDisabled) {
                ImGui::BeginDisabled();
            }

            auto &work = context.AvdCreationWork;
            const bool hasDeviceProfile = !work.DeviceProfiles.empty() && work.SelectedDevice >= 0 && work.SelectedDevice < static_cast<int>(work.DeviceProfiles.size());
            const bool hasImage = !work.SystemImages.empty() && work.SelectedSystemImage >= 0 && work.SelectedSystemImage < static_cast<int>(work.SystemImages.size());
            if (hasImage) {
                const auto &img = work.SystemImages[work.SelectedSystemImage];
                const std::string deviceId = hasDeviceProfile ? work.DeviceProfiles[work.SelectedDevice].Id : "Android";
                const std::string deviceName = hasDeviceProfile ? work.DeviceProfiles[work.SelectedDevice].Name : "Android Device";

                if (work.NameAutoFilled) {
                    const std::string base = deviceId + "_API_" + img.ApiLevel;
                    std::string sanitized;
                    sanitized.reserve(base.size());
                    for (const char c: base) {
                        const bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
                        sanitized.push_back(keep ? c : '_');
                    }
                    work.CreationData.Name = std::move(sanitized);
                }
                if (work.DisplayNameAutoFilled) {
                    work.CreationData.DisplayName = deviceName + " API " + img.ApiLevel;
                }
            }

            ImGui::Text("AVD Name");
            char nameBuffer[128];
            strncpy(nameBuffer, context.AvdCreationWork.CreationData.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##AvdName", "e.g. MyPixel7", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_CallbackCharFilter, AvdNameFilter)) {
                context.AvdCreationWork.CreationData.Name = nameBuffer;
                context.AvdCreationWork.NameAutoFilled = (nameBuffer[0] == '\0');
            }

            const bool nameConflict = AvdNameExists(
                context.Catalog.AvdNames,
                context.AvdCreationWork.CreationData.Name
            );

            if (nameConflict) {
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    " An AVD named \"%s\" already exists.",
                    context.AvdCreationWork.CreationData.Name.c_str()
                );
            }

            ImGui::Spacing();

            ImGui::Text("Display Name");
            char displayBuffer[128];
            strncpy(displayBuffer, context.AvdCreationWork.CreationData.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##DisplayName", "e.g. My Pixel 7", displayBuffer, sizeof(displayBuffer))) {
                context.AvdCreationWork.CreationData.DisplayName = displayBuffer;
                context.AvdCreationWork.DisplayNameAutoFilled = (displayBuffer[0] == '\0');
            }

            ImGui::Spacing();
            ImGui::Text("System Image");
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.SystemImages.empty()) {
                if (!context.Host.Sdk.SdkManagerPath.empty()) {
                    if (PickerButton("No system images available. Install one...", !formDisabled, ImVec2(-1.0F, 0.0F))) {
                        OpenSystemImagePicker(context);
                    }
                } else {
                    PickerButton("No system images installed", false, ImVec2(-1.0F, 0.0F));
                    ImGui::TextColored(
                        HexColor(Colors::NEGATIVE),
                        "SDK Manager was not found, so CoreDeck cannot install images automatically."
                    );
                }
            } else if (!context.AvdCreationWork.SystemImages.empty()) {
                const auto &systemImages = context.AvdCreationWork.SystemImages;
                const auto &selectedSystemImage = context.AvdCreationWork.SelectedSystemImage;
                const std::string preview = SystemImagePreviewLabel(systemImages[selectedSystemImage]);

                if (PickerButton(preview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    if (!context.Host.Sdk.SdkManagerPath.empty()) {
                        OpenSystemImagePicker(context);
                    }
                }
            } else {
                PickerButton("Loading system images...", false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();

            ImGui::Text("Device");
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.DeviceProfiles.empty()) {
                ImGui::TextDisabled("No device profiles found.");
                ImGui::TextWrapped("CoreDeck will use avdmanager's default hardware profile.");
            } else if (!context.AvdCreationWork.DeviceProfiles.empty()) {
                const auto &selectedDevice = context.AvdCreationWork.DeviceProfiles[context.AvdCreationWork.SelectedDevice];
                const std::string devicePreview = DeviceProfilePreviewLabel(selectedDevice);

                if (PickerButton(devicePreview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    context.AvdCreationWork.PendingSelectedDevice = context.AvdCreationWork.SelectedDevice;
                    context.AvdCreationWork.DeviceSearchFilter[0] = '\0';
                    context.AvdCreationWork.SelectedDeviceCategory = DeviceCategory::Phone;
                    context.UI.ShowDeviceProfileDialog = true;
                }
            } else {
                PickerButton("Loading device profiles...", false, ImVec2(-1.0F, 0.0F));
            }

            if (context.AvdCreationWork.Prefetch.Ready && hasDeviceProfile) {
                auto &skinWork = context.AvdCreationWork;
                if (skinWork.SkinAutoFilled && skinWork.SelectedDevice != skinWork.LastDeviceForSkinAuto) {
                    const auto &deviceId = skinWork.DeviceProfiles[skinWork.SelectedDevice].Id;
                    const auto match = FindSkinForDevice(skinWork.Skins, deviceId);
                    if (match.has_value()) {
                        for (int i = 0; i < static_cast<int>(skinWork.Skins.size()); i++) {
                            if (skinWork.Skins[i].Name == match->Name) {
                                skinWork.SelectedSkin = i + 1;
                                break;
                            }
                        }
                    } else {
                        skinWork.SelectedSkin = 0;
                    }
                    skinWork.LastDeviceForSkinAuto = skinWork.SelectedDevice;
                }
            }

            ImGui::Spacing();
            ImGui::Text("Skin");
            if (context.AvdCreationWork.Prefetch.Ready) {
                const std::string skinPreview = SkinPreviewLabel(context);
                if (PickerButton(skinPreview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    context.AvdCreationWork.PendingSelectedSkin = context.AvdCreationWork.SelectedSkin;
                    context.AvdCreationWork.SkinSearchFilter[0] = '\0';
                    context.UI.ShowSkinDialog = true;
                }
            } else {
                PickerButton("Loading skins...", false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();
            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float colWidth = (ImGui::GetContentRegionAvail().x - rowSpacing) * 0.5F;
            const float col2X = ImGui::GetCursorPosX() + colWidth + rowSpacing;

            ImGui::Text("RAM (MB)");
            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);
            ImGui::Text("SD Card Size");

            char ramBuffer[32];
            strncpy(ramBuffer, context.AvdCreationWork.CreationData.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##ram", "e.g. 2048 (MB)", ramBuffer, sizeof(ramBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.RamSize = ramBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);

            char sdBuffer[32];
            strncpy(sdBuffer, context.AvdCreationWork.CreationData.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##sdcard", "e.g. 512 (MB)", sdBuffer, sizeof(sdBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("GPU Mode");
            const auto &gpuModes = GpuModeOptions();
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::BeginCombo("##gpu", gpuModes[context.AvdCreationWork.SelectedGpuMode].Label)) {
                for (int i = 0; i < static_cast<int>(gpuModes.size()); i++) {
                    const bool isSelected = context.AvdCreationWork.SelectedGpuMode == i;
                    if (ImGui::Selectable(gpuModes[i].Label, isSelected)) {
                        context.AvdCreationWork.SelectedGpuMode = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (formDisabled) {
                ImGui::EndDisabled();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5F;

            const bool canCreate = !context.AvdCreationWork.CreationData.Name.empty() && hasImage && !nameConflict && !formDisabled;

            if (isCreating) {
                ImGui::BeginDisabled();
                PositiveButton("Creating...", false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                if (PositiveButton("Create", canCreate, ImVec2(halfWidth, 0))) {
                    const auto &systemImagePackagePath = context.AvdCreationWork.SystemImages[context.AvdCreationWork.SelectedSystemImage].PackagePath;

                    context.AvdCreationWork.CreationData.SystemImagePackagePath = systemImagePackagePath;
                    context.AvdCreationWork.CreationData.DeviceId =
                        hasDeviceProfile
                            ? context.AvdCreationWork.DeviceProfiles[context.AvdCreationWork.SelectedDevice].Id
                            : "";
                    context.AvdCreationWork.CreationData.GpuMode = gpuModes[context.AvdCreationWork.SelectedGpuMode].Value;
                    if (context.AvdCreationWork.SelectedSkin > 0 &&
                        context.AvdCreationWork.SelectedSkin - 1 < static_cast<int>(context.AvdCreationWork.Skins.size())) {
                        const auto &chosenSkin = context.AvdCreationWork.Skins[context.AvdCreationWork.SelectedSkin - 1];
                        context.AvdCreationWork.CreationData.SkinName = chosenSkin.Name;
                        context.AvdCreationWork.CreationData.SkinPath = chosenSkin.Path;
                    } else {
                        context.AvdCreationWork.CreationData.SkinName.clear();
                        context.AvdCreationWork.CreationData.SkinPath.clear();
                    }
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

            BuildDeviceProfileWindow(context);
            BuildSkinWindow(context);
            BuildInstallImageWindow(context);

            ImGui::EndPopup();
        }
    }
}

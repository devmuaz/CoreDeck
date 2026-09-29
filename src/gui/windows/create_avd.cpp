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
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
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

        int DefaultPhoneProfileIndex(const std::vector<DeviceProfile> &profiles) {
            int firstPhone = -1;
            for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
                if (profiles.at(i).Id == "medium_phone") {
                    return i;
                }
                if (firstPhone < 0 && DeviceCategoryForProfile(profiles.at(i)) == DeviceCategory::Phone) {
                    firstPhone = i;
                }
            }
            return firstPhone;
        }
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildCreateAvdWindow(Context &context) {
        const std::string title = TrWindow("Create New AVD", "CreateAvdDialog");
        if (!context.UI.ShowCreateAvdDialog && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        if (BeginCenteredModal(title.c_str(), &context.UI.ShowCreateAvdDialog, ImVec2(Em(70.0F), 0), WINDOW_AUTO_RESIZE_FLAGS)) {
            const bool isLoading = context.AvdCreationWork.Prefetch.Loading.load();
            const bool isCreating = context.Jobs.AvdCreation.Busy.load();
            const bool formDisabled = isLoading || isCreating;

            if (formDisabled) {
                ImGui::BeginDisabled();
            }

            auto &work = context.AvdCreationWork;
            if (work.Prefetch.Ready && work.DeviceAutoFilled && !work.DeviceProfiles.empty()) {
                const int phoneIndex = DefaultPhoneProfileIndex(work.DeviceProfiles);
                if (phoneIndex >= 0 && work.SelectedDevice != phoneIndex) {
                    work.SelectedDevice = phoneIndex;
                    work.PendingSelectedDevice = phoneIndex;
                }
            }
            const bool hasDeviceProfile = !work.DeviceProfiles.empty() && work.SelectedDevice >= 0 && work.SelectedDevice < static_cast<int>(work.DeviceProfiles.size());
            const bool hasImage = !work.SystemImages.empty() && work.SelectedSystemImage >= 0 && work.SelectedSystemImage < static_cast<int>(work.SystemImages.size());
            if (hasImage) {
                const auto &img = work.SystemImages.at(work.SelectedSystemImage);
                const std::string deviceId = hasDeviceProfile ? work.DeviceProfiles.at(work.SelectedDevice).Id : "Android";
                const std::string deviceName = hasDeviceProfile ? work.DeviceProfiles.at(work.SelectedDevice).Name : Tr("Android Device");

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
                    work.CreationData.DisplayName = TrFormat("{0} API {1}", deviceName, img.ApiLevel);
                }
            }

            ImGui::Text("%s", Tr("System Image"));
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.SystemImages.empty()) {
                if (!context.Host.Sdk.SdkManagerPath.empty()) {
                    if (PickerButton(Tr("No system images available. Install one..."), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                        OpenInstallImageDialog(context);
                    }
                } else {
                    PickerButton(Tr("No system images installed"), false, ImVec2(-1.0F, 0.0F));
                    StatusMessage(
                        StatusMessageTone::Error,
                        Tr("SDK Manager was not found. Run 'Tools > Health Check' to fix this.")
                    );
                }
            } else if (!context.AvdCreationWork.SystemImages.empty()) {
                const auto &systemImages = context.AvdCreationWork.SystemImages;
                const auto &selectedSystemImage = context.AvdCreationWork.SelectedSystemImage;
                const std::string preview = SystemImagePreviewLabel(systemImages.at(selectedSystemImage));

                if (PickerButton(preview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    if (!context.Host.Sdk.SdkManagerPath.empty()) {
                        OpenInstallImageDialog(context);
                    }
                }
            } else {
                PickerButton(Tr("Loading system images..."), false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();

            ImGui::Text("%s", Tr("Device"));
            if (context.AvdCreationWork.Prefetch.Ready && context.AvdCreationWork.DeviceProfiles.empty()) {
                StatusMessage(
                    StatusMessageTone::Info,
                    Tr("No device profiles found. Using default hardware profile.")
                );
            } else if (!context.AvdCreationWork.DeviceProfiles.empty()) {
                const auto &selectedDevice = context.AvdCreationWork.DeviceProfiles.at(context.AvdCreationWork.SelectedDevice);
                const std::string devicePreview = DeviceProfilePreviewLabel(selectedDevice);

                if (PickerButton(devicePreview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    context.AvdCreationWork.PendingSelectedDevice = context.AvdCreationWork.SelectedDevice;
                    context.AvdCreationWork.DeviceSearchFilter[0] = '\0';
                    context.AvdCreationWork.SelectedDeviceCategory = DeviceCategory::Phone;
                    context.UI.ShowDeviceProfileDialog = true;
                }
            } else {
                PickerButton(Tr("Loading device profiles..."), false, ImVec2(-1.0F, 0.0F));
            }

            if (context.AvdCreationWork.Prefetch.Ready &&
                context.AvdCreationWork.DeviceDefinitionsSkipped &&
                !context.AvdCreationWork.DeviceProfiles.empty()) {
                StatusMessage(
                    StatusMessageTone::Warning,
                    Tr("Some system-image device definitions could not be loaded.")
                );
            }

            if (context.AvdCreationWork.Prefetch.Ready && hasDeviceProfile) {
                auto &skinWork = context.AvdCreationWork;
                if (skinWork.SkinAutoFilled && skinWork.SelectedDevice != skinWork.LastDeviceForSkinAuto) {
                    const auto &deviceId = skinWork.DeviceProfiles.at(skinWork.SelectedDevice).Id;
                    const auto match = FindSkinForDevice(skinWork.Skins, deviceId);
                    if (match.has_value()) {
                        for (int i = 0; i < static_cast<int>(skinWork.Skins.size()); i++) {
                            if (skinWork.Skins.at(i).Name == match->Name) {
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
            ImGui::Text("%s", Tr("Skin"));
            if (context.AvdCreationWork.Prefetch.Ready) {
                const std::string skinPreview = SkinPreviewLabel(context);
                if (PickerButton(skinPreview.c_str(), !formDisabled, ImVec2(-1.0F, 0.0F))) {
                    context.AvdCreationWork.PendingSelectedSkin = context.AvdCreationWork.SelectedSkin;
                    context.AvdCreationWork.SkinSearchFilter[0] = '\0';
                    context.UI.ShowSkinDialog = true;
                }
            } else {
                PickerButton(Tr("Loading skins..."), false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();

            ImGui::Text("%s", Tr("AVD Name"));
            char nameBuffer[128];
            strncpy(nameBuffer, context.AvdCreationWork.CreationData.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##AvdName", Tr("e.g. MyPixel7"), nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_CallbackCharFilter, AvdNameFilter)) {
                context.AvdCreationWork.CreationData.Name = nameBuffer;
                context.AvdCreationWork.NameAutoFilled = (nameBuffer[0] == '\0');
            }

            const bool nameConflict = AvdNameExists(
                context.Catalog.AvdNames,
                context.AvdCreationWork.CreationData.Name
            );

            if (nameConflict) {
                const std::string message = TrFormat(
                    "An AVD named \"{0}\" already exists.",
                    context.AvdCreationWork.CreationData.Name
                );
                StatusMessage(StatusMessageTone::Error, message.c_str());
            }

            ImGui::Spacing();

            ImGui::Text("%s", Tr("Display Name"));
            char displayBuffer[128];
            strncpy(displayBuffer, context.AvdCreationWork.CreationData.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##DisplayName", Tr("e.g. My Pixel 7"), displayBuffer, sizeof(displayBuffer))) {
                context.AvdCreationWork.CreationData.DisplayName = displayBuffer;
                context.AvdCreationWork.DisplayNameAutoFilled = (displayBuffer[0] == '\0');
            }

            ImGui::Spacing();
            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float colWidth = (ImGui::GetContentRegionAvail().x - rowSpacing) * 0.5F;
            const float col2X = ImGui::GetCursorPosX() + colWidth + rowSpacing;

            ImGui::Text("%s", Tr("RAM (MB)"));
            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);
            ImGui::Text("%s", Tr("SD Card Size"));

            char ramBuffer[32];
            strncpy(ramBuffer, context.AvdCreationWork.CreationData.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##ram", Tr("e.g. 2048 (MB)"), ramBuffer, sizeof(ramBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.RamSize = ramBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);

            char sdBuffer[32];
            strncpy(sdBuffer, context.AvdCreationWork.CreationData.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##sdcard", Tr("e.g. 512 (MB)"), sdBuffer, sizeof(sdBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.AvdCreationWork.CreationData.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("%s", Tr("GPU Mode"));
            const auto &gpuModes = GpuModeOptions();
            ImGui::SetNextItemWidth(-1.0F);
            {
                ComboStyle cs;
                if (ImGui::BeginCombo("##gpu", gpuModes.at(context.AvdCreationWork.SelectedGpuMode).Label)) {
                    for (int i = 0; i < static_cast<int>(gpuModes.size()); i++) {
                        const bool isSelected = context.AvdCreationWork.SelectedGpuMode == i;
                        if (RoundedSelectable(gpuModes.at(i).Label, isSelected)) {
                            context.AvdCreationWork.SelectedGpuMode = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (formDisabled) {
                ImGui::EndDisabled();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            const bool canCreate = !context.AvdCreationWork.CreationData.Name.empty() && hasImage && !nameConflict && !formDisabled;

            if (isCreating) {
                ImGui::BeginDisabled();
                PositiveButton(Tr("Creating..."), false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                if (PositiveButton(Tr("Create"), canCreate, ImVec2(halfWidth, 0))) {
                    const auto &systemImagePackagePath = context.AvdCreationWork.SystemImages.at(context.AvdCreationWork.SelectedSystemImage).PackagePath;

                    context.AvdCreationWork.CreationData.SystemImagePackagePath = systemImagePackagePath;
                    context.AvdCreationWork.CreationData.DeviceId =
                        hasDeviceProfile
                            ? context.AvdCreationWork.DeviceProfiles.at(context.AvdCreationWork.SelectedDevice).Id
                            : "";
                    context.AvdCreationWork.CreationData.GpuMode = gpuModes.at(context.AvdCreationWork.SelectedGpuMode).Value;
                    if (context.AvdCreationWork.SelectedSkin > 0 &&
                        context.AvdCreationWork.SelectedSkin - 1 < static_cast<int>(context.AvdCreationWork.Skins.size())) {
                        const auto &chosenSkin = context.AvdCreationWork.Skins.at(context.AvdCreationWork.SelectedSkin - 1);
                        context.AvdCreationWork.CreationData.SkinName = chosenSkin.Name;
                        context.AvdCreationWork.CreationData.SkinPath = chosenSkin.Path;
                    } else {
                        context.AvdCreationWork.CreationData.SkinName.clear();
                        context.AvdCreationWork.CreationData.SkinPath.clear();
                    }
                    if (!context.AvdCreationWork.CreationData.SdCardSize.empty()) {
                        context.AvdCreationWork.CreationData.SdCardSize += 'M';
                    }

                    context.Jobs.AvdCreation.Busy = true;
                    context.Jobs.AvdCreation.Future = std::async(std::launch::async, [&context] {
                        CreateAvd(context.Host.Sdk, context.AvdCreationWork.CreationData);
                        context.Jobs.AvdCreation.Busy = false;
                    });
                }
            }
            ImGui::SameLine();
            if (PrimaryButton(Tr("Cancel"), !isCreating, ImVec2(halfWidth, 0))) {
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

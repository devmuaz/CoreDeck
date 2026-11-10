//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include <future>
#include <utility>
#include "imgui.h"

#include "gui/home/create_avd_dialog.h"
#include "gui/home/device_profile_dialog.h"
#include "gui/home/install_image_dialog.h"
#include "gui/home/skin_dialog.h"
#include "gui/shell/application.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        // ImGui's callback type takes a non-const data pointer.
        int DigitsOnlyFilter(ImGuiInputTextCallbackData *data) { // NOLINT(misc-const-correctness)
            return data->EventChar >= '0' && data->EventChar <= '9' ? 0 : 1;
        }


        int AvdNameFilter(ImGuiInputTextCallbackData *data) { // NOLINT(misc-const-correctness)
            const ImWchar c = data->EventChar;
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            return ok ? 0 : 1;
        }

        bool AvdNameExists(const std::vector<std::string> &names, const std::string &candidate) {
            if (candidate.empty()) {
                return false;
            }
            const std::string needle = Platform::LowerCopy(candidate);
            return std::ranges::any_of(names, [&](const std::string &n) { return Platform::LowerCopy(n) == needle; });
        }

        int DefaultPhoneProfileIndex(const std::vector<Core::DeviceProfile> &profiles) {
            int firstPhone = -1;
            for (int i = 0; std::cmp_less(i, profiles.size()); ++i) {
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

    void OpenCreateAvdDialog(Context &context) {
        context.Dialogs.CreateAvd.State.CreationData = {};
        context.Dialogs.CreateAvd.State.SelectedSystemImage = 0;
        context.Dialogs.CreateAvd.State.SelectedDevice = 0;
        context.Dialogs.CreateAvd.State.SelectedGpuMode = 0;
        context.Dialogs.CreateAvd.State.NameAutoFilled = true;
        context.Dialogs.CreateAvd.State.DisplayNameAutoFilled = true;
        context.Dialogs.CreateAvd.State.DeviceAutoFilled = true;
        context.Dialogs.CreateAvd.State.PendingSelectedDevice = 0;
        context.Dialogs.CreateAvd.State.SkinAutoFilled = true;
        context.Dialogs.CreateAvd.State.SelectedSkin = 0;
        context.Dialogs.CreateAvd.State.PendingSelectedSkin = 0;
        context.Dialogs.CreateAvd.State.LastDeviceForSkinAuto = -1;
        context.Dialogs.CreateAvd.State.SkinSearchFilter[0] = '\0';
        context.Dialogs.CreateAvd.State.DeviceDefinitionsSkipped = false;
        context.Dialogs.CreateAvd.State.Prefetch.Ready = false;
        context.Dialogs.CreateAvd.State.Prefetch.Loading = true;
        context.Dialogs.CreateAvd.Open = true;

        context.Dialogs.CreateAvd.State.Prefetch.Future = std::async(std::launch::async, [&context] {
            auto images = Core::ListSystemImages(context.Host.Sdk);
            auto devices = Core::ListDeviceProfiles(context.Host.Sdk);
            auto skins = Core::ListSkins(context.Host.Sdk);
            context.Dialogs.CreateAvd.State.SystemImages = std::move(images);
            context.Dialogs.CreateAvd.State.DeviceDefinitionsSkipped = devices.SkippedDeviceDefinitions;
            context.Dialogs.CreateAvd.State.DeviceProfiles = std::move(devices.Profiles);
            context.Dialogs.CreateAvd.State.Skins = std::move(skins);
            context.Dialogs.CreateAvd.State.Prefetch.Loading = false;
            context.Dialogs.CreateAvd.State.Prefetch.Ready = true;
        });
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildCreateAvdDialog(Context &context) {
        const std::string title = Platform::TrWindow("Create New AVD", "CreateAvdDialog");
        if (!context.Dialogs.CreateAvd.Open && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        if (BeginCenteredModal(title.c_str(), &context.Dialogs.CreateAvd.Open, ImVec2(Em(70.0F), 0), WINDOW_AUTO_RESIZE_FLAGS)) {
            const bool isLoading = !context.Dialogs.CreateAvd.State.Prefetch.Ready.load();
            LoadingOverlay loading(isLoading);

            const bool isCreating = context.Workspaces.Home.Jobs.AvdCreation.Busy.load();
            const auto &gpuModes = Core::GpuModeOptions();

            if (isCreating) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            }

            auto &work = context.Dialogs.CreateAvd.State;
            if (work.Prefetch.Ready && work.DeviceAutoFilled && !work.DeviceProfiles.empty()) {
                const int phoneIndex = DefaultPhoneProfileIndex(work.DeviceProfiles);
                if (phoneIndex >= 0 && work.SelectedDevice != phoneIndex) {
                    work.SelectedDevice = phoneIndex;
                    work.PendingSelectedDevice = phoneIndex;
                }
            }
            const bool hasDeviceProfile = !work.DeviceProfiles.empty() && work.SelectedDevice >= 0 && std::cmp_less(work.SelectedDevice, work.DeviceProfiles.size());
            const bool hasImage = !work.SystemImages.empty() && work.SelectedSystemImage >= 0 && std::cmp_less(work.SelectedSystemImage, work.SystemImages.size());
            if (hasImage) {
                const auto &img = work.SystemImages.at(work.SelectedSystemImage);
                const std::string deviceId = hasDeviceProfile ? work.DeviceProfiles.at(work.SelectedDevice).Id : "Android";
                const std::string deviceName = hasDeviceProfile ? work.DeviceProfiles.at(work.SelectedDevice).Name : Platform::Tr("Android Device");

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
                    work.CreationData.DisplayName = Platform::TrFormat("{0} API {1}", deviceName, img.ApiLevel);
                }
            }

            ImGui::Text("%s", Platform::Tr("System Image"));
            if (context.Dialogs.CreateAvd.State.Prefetch.Ready && context.Dialogs.CreateAvd.State.SystemImages.empty()) {
                if (!context.Host.Sdk.SdkManagerPath.empty()) {
                    if (PickerButton(Platform::Tr("No system images available. Install one..."), true, ImVec2(-1.0F, 0.0F))) {
                        OpenInstallImageDialog(context);
                    }
                } else {
                    PickerButton(Platform::Tr("No system images installed"), false, ImVec2(-1.0F, 0.0F));
                    StatusMessage(
                        Tone::Negative,
                        Platform::Tr("SDK Manager was not found. Run 'Tools > Health Check' to fix this.")
                    );
                }
            } else if (!context.Dialogs.CreateAvd.State.SystemImages.empty()) {
                const auto &systemImages = context.Dialogs.CreateAvd.State.SystemImages;
                const auto &selectedSystemImage = context.Dialogs.CreateAvd.State.SelectedSystemImage;
                const std::string preview = SystemImagePreviewLabel(systemImages.at(selectedSystemImage));

                if (PickerButton(preview.c_str(), true, ImVec2(-1.0F, 0.0F)) && !context.Host.Sdk.SdkManagerPath.empty()) {
                    OpenInstallImageDialog(context);
                }

            } else {
                PickerButton(Platform::Tr("Loading system images..."), false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();

            ImGui::Text("%s", Platform::Tr("Device"));
            if (context.Dialogs.CreateAvd.State.Prefetch.Ready && context.Dialogs.CreateAvd.State.DeviceProfiles.empty()) {
                StatusMessage(
                    Tone::Info,
                    Platform::Tr("No device profiles found. Using default hardware profile.")
                );
            } else if (!context.Dialogs.CreateAvd.State.DeviceProfiles.empty()) {
                const auto &selectedDevice = context.Dialogs.CreateAvd.State.DeviceProfiles.at(context.Dialogs.CreateAvd.State.SelectedDevice);
                const std::string devicePreview = DeviceProfilePreviewLabel(selectedDevice);

                if (PickerButton(devicePreview.c_str(), true, ImVec2(-1.0F, 0.0F))) {
                    context.Dialogs.CreateAvd.State.PendingSelectedDevice = context.Dialogs.CreateAvd.State.SelectedDevice;
                    context.Dialogs.CreateAvd.State.DeviceSearchFilter[0] = '\0';
                    context.Dialogs.CreateAvd.State.SelectedDeviceCategory = DeviceCategory::Phone;
                    OpenDeviceProfileDialog(context);
                }
            } else {
                PickerButton(Platform::Tr("Loading device profiles..."), false, ImVec2(-1.0F, 0.0F));
            }

            if (context.Dialogs.CreateAvd.State.Prefetch.Ready &&
                context.Dialogs.CreateAvd.State.DeviceDefinitionsSkipped &&
                !context.Dialogs.CreateAvd.State.DeviceProfiles.empty()) {
                StatusMessage(
                    Tone::Warning,
                    Platform::Tr("Some system-image device definitions could not be loaded.")
                );
            }

            if (context.Dialogs.CreateAvd.State.Prefetch.Ready && hasDeviceProfile) {
                auto &skinWork = context.Dialogs.CreateAvd.State;
                if (skinWork.SkinAutoFilled && skinWork.SelectedDevice != skinWork.LastDeviceForSkinAuto) {
                    const auto &deviceId = skinWork.DeviceProfiles.at(skinWork.SelectedDevice).Id;
                    const auto match = Core::FindSkinForDevice(skinWork.Skins, deviceId);
                    if (match.has_value()) {
                        for (int i = 0; std::cmp_less(i, skinWork.Skins.size()); i++) {
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
            ImGui::Text("%s", Platform::Tr("Skin"));
            if (context.Dialogs.CreateAvd.State.Prefetch.Ready) {
                const std::string skinPreview = SkinPreviewLabel(context);
                if (PickerButton(skinPreview.c_str(), true, ImVec2(-1.0F, 0.0F))) {
                    context.Dialogs.CreateAvd.State.PendingSelectedSkin = context.Dialogs.CreateAvd.State.SelectedSkin;
                    context.Dialogs.CreateAvd.State.SkinSearchFilter[0] = '\0';
                    OpenSkinDialog(context);
                }
            } else {
                PickerButton(Platform::Tr("Loading skins..."), false, ImVec2(-1.0F, 0.0F));
            }

            ImGui::Spacing();

            ImGui::Text("%s", Platform::Tr("AVD Name"));
            char nameBuffer[128];
            CopyToBuffer(nameBuffer, sizeof(nameBuffer), context.Dialogs.CreateAvd.State.CreationData.Name);
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##AvdName", Platform::Tr("e.g. MyPixel7"), nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_CallbackCharFilter, AvdNameFilter)) {
                context.Dialogs.CreateAvd.State.CreationData.Name = nameBuffer;
                context.Dialogs.CreateAvd.State.NameAutoFilled = (nameBuffer[0] == '\0');
            }

            const bool nameConflict = AvdNameExists(
                context.Workspaces.Home.Catalog.AvdNames,
                context.Dialogs.CreateAvd.State.CreationData.Name
            );

            if (nameConflict) {
                const std::string message = Platform::TrFormat(
                    "An AVD named \"{0}\" already exists.",
                    context.Dialogs.CreateAvd.State.CreationData.Name
                );
                StatusMessage(Tone::Negative, message.c_str());
            }

            ImGui::Spacing();

            ImGui::Text("%s", Platform::Tr("Display Name"));
            char displayBuffer[128];
            CopyToBuffer(displayBuffer, sizeof(displayBuffer), context.Dialogs.CreateAvd.State.CreationData.DisplayName);
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::InputTextWithHint("##DisplayName", Platform::Tr("e.g. My Pixel 7"), displayBuffer, sizeof(displayBuffer))) {
                context.Dialogs.CreateAvd.State.CreationData.DisplayName = displayBuffer;
                context.Dialogs.CreateAvd.State.DisplayNameAutoFilled = (displayBuffer[0] == '\0');
            }

            ImGui::Spacing();
            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float colWidth = (ImGui::GetContentRegionAvail().x - rowSpacing) * 0.5F;
            const float col2X = ImGui::GetCursorPosX() + colWidth + rowSpacing;

            ImGui::Text("%s", Platform::Tr("RAM (MB)"));
            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);
            ImGui::Text("%s", Platform::Tr("SD Card Size"));

            char ramBuffer[32];
            CopyToBuffer(ramBuffer, sizeof(ramBuffer), context.Dialogs.CreateAvd.State.CreationData.RamSize);
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##ram", Platform::Tr("e.g. 2048 (MB)"), ramBuffer, sizeof(ramBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.Dialogs.CreateAvd.State.CreationData.RamSize = ramBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);

            char sdBuffer[32];
            CopyToBuffer(sdBuffer, sizeof(sdBuffer), context.Dialogs.CreateAvd.State.CreationData.SdCardSize);
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##sdcard", Platform::Tr("e.g. 512 (MB)"), sdBuffer, sizeof(sdBuffer), ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.Dialogs.CreateAvd.State.CreationData.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("%s", Platform::Tr("GPU Mode"));
            std::vector<const char *> gpuLabels;
            gpuLabels.reserve(gpuModes.size());
            for (const auto &mode: gpuModes) {
                gpuLabels.push_back(mode.Label);
            }
            RoundedCombo(
                "##gpu",
                gpuModes.at(context.Dialogs.CreateAvd.State.SelectedGpuMode).Label,
                gpuLabels.data(),
                static_cast<int>(gpuLabels.size()),
                context.Dialogs.CreateAvd.State.SelectedGpuMode
            );

            if (isCreating) {
                ImGui::PopItemFlag();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);
            const bool canCreate = !context.Dialogs.CreateAvd.State.CreationData.Name.empty() && hasImage && !nameConflict;

            if (isCreating) {
                PositiveButton(Platform::Tr("Creating..."), true, ImVec2(halfWidth, 0), true);
            } else if (PositiveButton(Platform::Tr("Create"), canCreate, ImVec2(halfWidth, 0))) {
                const auto &systemImagePackagePath = context.Dialogs.CreateAvd.State.SystemImages.at(context.Dialogs.CreateAvd.State.SelectedSystemImage).PackagePath;

                context.Dialogs.CreateAvd.State.CreationData.SystemImagePackagePath = systemImagePackagePath;
                context.Dialogs.CreateAvd.State.CreationData.DeviceId =
                    hasDeviceProfile
                        ? context.Dialogs.CreateAvd.State.DeviceProfiles.at(context.Dialogs.CreateAvd.State.SelectedDevice).Id
                        : "";
                context.Dialogs.CreateAvd.State.CreationData.GpuMode = gpuModes.at(context.Dialogs.CreateAvd.State.SelectedGpuMode).Value;
                if (context.Dialogs.CreateAvd.State.SelectedSkin > 0 &&
                    context.Dialogs.CreateAvd.State.SelectedSkin - 1 < static_cast<int>(context.Dialogs.CreateAvd.State.Skins.size())) {
                    const auto &chosenSkin = context.Dialogs.CreateAvd.State.Skins.at(context.Dialogs.CreateAvd.State.SelectedSkin - 1);
                    context.Dialogs.CreateAvd.State.CreationData.SkinName = chosenSkin.Name;
                    context.Dialogs.CreateAvd.State.CreationData.SkinPath = chosenSkin.Path;
                } else {
                    context.Dialogs.CreateAvd.State.CreationData.SkinName.clear();
                    context.Dialogs.CreateAvd.State.CreationData.SkinPath.clear();
                }
                if (!context.Dialogs.CreateAvd.State.CreationData.SdCardSize.empty()) {
                    context.Dialogs.CreateAvd.State.CreationData.SdCardSize += 'M';
                }

                context.Workspaces.Home.Jobs.AvdCreation.Busy = true;
                context.Workspaces.Home.Jobs.AvdCreation.Future = std::async(std::launch::async, [&context] {
                    Core::CreateAvd(context.Host.Sdk, context.Dialogs.CreateAvd.State.CreationData);
                    context.Workspaces.Home.Jobs.AvdCreation.Busy = false;
                });
            }
            ImGui::SameLine();
            if (PrimaryButton(Platform::Tr("Cancel"), !isCreating, ImVec2(halfWidth, 0))) {
                context.Dialogs.CreateAvd.Open = false;
                ImGui::CloseCurrentPopup();
            }

            if (!context.Workspaces.Home.Jobs.AvdCreation.Busy && context.Workspaces.Home.Jobs.AvdCreation.Future.valid()) {
                context.Workspaces.Home.Jobs.AvdCreation.Future.get();
                context.Dialogs.CreateAvd.Open = false;
                ImGui::CloseCurrentPopup();
                RefreshAvds(context);
            }

            BuildDeviceProfileDialog(context);
            BuildSkinDialog(context);
            BuildInstallImageDialog(context);

            loading.End();
            ImGui::EndPopup();
        }
    }
}

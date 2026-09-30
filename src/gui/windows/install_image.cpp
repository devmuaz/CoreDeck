//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <chrono>
#include <string>
#include "imgui.h"

#include "install_image.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/constants.h"
#include "../../core/sdk_manager.h"
#include "../../core/utilities.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        ImageCategory CategoryForImage(const RemoteSystemImage &img) {
            const std::string searchable = LowerCopy(StrConcat(img.PackagePath, " ", img.Variant, " ", img.DisplayName));

            if (searchable.find("wear") != std::string::npos) {
                return ImageCategory::Wear;
            }
            if (searchable.find("automotive") != std::string::npos || searchable.find("android-auto") != std::string::npos) {
                return ImageCategory::Automotive;
            }
            if (searchable.find("desktop") != std::string::npos) {
                return ImageCategory::Desktop;
            }
            if (searchable.find("xr") != std::string::npos) {
                return ImageCategory::Xr;
            }
            if (searchable.find("android-tv") != std::string::npos ||
                searchable.find("google-tv") != std::string::npos ||
                searchable.find("_tv") != std::string::npos ||
                searchable.find(";tv") != std::string::npos) {
                return ImageCategory::Tv;
            }
            if (img.Variant == "default" ||
                img.Variant.starts_with("google_apis") ||
                img.Variant.starts_with("aosp_atd") ||
                img.Variant.starts_with("google_atd")) {
                return ImageCategory::PhoneTablet;
            }
            return ImageCategory::Other;
        }

        bool MatchesImageCategory(const RemoteSystemImage &img, const ImageCategory category) {
            return category == ImageCategory::All || CategoryForImage(img) == category;
        }

        const char *SystemImagePageSizeLabel(const std::string &variant) {
            return variant.find("ps16k") != std::string::npos ? Tr("16 KB") : Tr("4 KB");
        }

        bool MatchesImageFilter(const RemoteSystemImage &img, const char *filter) {
            if (!filter || filter[0] == '\0') {
                return true;
            }

            const auto searchable = StrConcat(
                img.DisplayName,
                " ",
                img.ApiLevel,
                " ",
                img.Variant,
                " ",
                img.Abi,
                " ",
                img.PackagePath,
                " ",
                SystemImagePageSizeLabel(img.Variant)
            );
            return ContainsIgnoreCase(searchable, filter);
        }

        bool MatchesImageFilters(const RemoteSystemImage &img, const char *filter, const ImageCategory category) {
            return MatchesImageCategory(img, category) && MatchesImageFilter(img, filter);
        }

        void StartInstall(Context &context, const std::string &pkgPath) {
            auto &work = context.ImageInstallationWork;
            work.Progress = std::make_shared<InstallProgressData>();
            work.Installing = true;
            auto progress = work.Progress;
            const SdkInfo sdk = context.Host.Sdk;
            work.InstallFuture = std::async(
                std::launch::async,
                [&context, sdk, pkgPath, progress] {
                    const bool ok = InstallSystemImage(sdk, pkgPath, progress);
                    context.ImageInstallationWork.Installing = false;
                    return ok;
                }
            );
        }

        bool SelectInstalledSystemImage(Context &context, const std::string &packagePath) {
            auto &images = context.AvdCreationWork.SystemImages;
            for (int i = 0; i < static_cast<int>(images.size()); i++) {
                if (images.at(static_cast<std::size_t>(i)).PackagePath == packagePath) {
                    context.AvdCreationWork.SelectedSystemImage = i;
                    return true;
                }
            }

            images = ListSystemImages(context.Host.Sdk);
            for (int i = 0; i < static_cast<int>(images.size()); i++) {
                if (images.at(static_cast<std::size_t>(i)).PackagePath == packagePath) {
                    context.AvdCreationWork.SelectedSystemImage = i;
                    return true;
                }
            }

            return false;
        }

        void RefreshSystemImageLists(Context &context) {
            auto &images = context.AvdCreationWork.SystemImages;
            images = ListSystemImages(context.Host.Sdk);
            if (images.empty()) {
                context.AvdCreationWork.SelectedSystemImage = 0;
            } else {
                context.AvdCreationWork.SelectedSystemImage = std::clamp(
                    context.AvdCreationWork.SelectedSystemImage,
                    0,
                    static_cast<int>(images.size()) - 1
                );
            }

            context.ImageInstallationWork.RemoteImages = ListRemoteSystemImages(context.Host.Sdk, images);
        }
    }

    LabeledIconStyle SystemImageTypeStyleForVariant(const std::string &variant) {
        if (variant.starts_with("google_apis_playstore")) {
            return {.Icon = Icons::PLAY, .Label = Tr("Google Play"), .Color = Colors::POSITIVE};
        }
        if (variant.starts_with("google_apis")) {
            return {.Icon = Icons::GEAR, .Label = Tr("Google APIs"), .Color = Colors::ACCENT_PHONE};
        }
        if (variant.starts_with("aosp_atd") || variant.starts_with("google_atd")) {
            return {.Icon = Icons::MOBILE, .Label = "ATD", .Color = Colors::ACCENT_WEAR};
        }
        return {.Icon = Icons::MOBILE, .Label = Tr("Default"), .Color = Colors::TEXT_SUBTLE};
    }

    LabeledIconStyle SystemImageTypeStyleFor(const SystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    LabeledIconStyle SystemImageTypeStyleFor(const RemoteSystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    std::string SystemImageDisplayName(const std::string &apiLevel, const std::string &fallback) {
        if (!apiLevel.empty()) {
            return TrFormat("Android {0}", apiLevel);
        }
        return fallback;
    }

    std::string SystemImagePreviewLabel(const SystemImage &img) {
        const auto style = SystemImageTypeStyleFor(img);
        return StrConcat(
            SystemImageDisplayName(img.ApiLevel, img.DisplayName),
            " - ",
            style.Label,
            " - ",
            SystemImagePageSizeLabel(img.Variant),
            " - ",
            img.Abi
        );
    }

    void OpenInstallImageDialog(Context &context) {
        context.ImageInstallationWork.SelectedImage = -1;
        context.ImageInstallationWork.SelectedCategory = ImageCategory::PhoneTablet;
        context.ImageInstallationWork.SearchFilter[0] = '\0';
        context.ImageInstallationWork.Progress.reset();
        context.ImageInstallationWork.Prefetch.Ready = false;
        context.ImageInstallationWork.Prefetch.Loading = true;
        context.UI.ShowInstallImageDialog = true;

        const SdkInfo sdk = context.Host.Sdk;
        context.ImageInstallationWork.Prefetch.Future = std::async(std::launch::async, [&context, sdk] {
            const auto localImages = ListSystemImages(sdk);
            auto remoteImages = ListRemoteSystemImages(sdk, localImages);
            context.AvdCreationWork.SystemImages = localImages;
            context.ImageInstallationWork.RemoteImages = std::move(remoteImages);
            context.ImageInstallationWork.Prefetch.Loading = false;
            context.ImageInstallationWork.Prefetch.Ready = true;
        });
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildInstallImageWindow(Context &context) {
        if (context.UI.ShowInstallImageDialog) {
            const std::string title = TrWindow("Install System Image", "InstallImageDialog");
            const bool installing = context.ImageInstallationWork.Installing.load();
            const bool removalBusy = context.AvdCreationWork.SystemImageRemoval.Busy.load();
            bool *pOpen = (installing || removalBusy) ? nullptr : &context.UI.ShowInstallImageDialog;

            if (BeginCenteredModal(title.c_str(), pOpen, EmV(100.0F, 28.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
                auto &work = context.ImageInstallationWork;
                auto &removal = context.AvdCreationWork.SystemImageRemoval;
                const bool isLoading = work.Prefetch.Loading.load();
                const bool isInstalling = installing;

                if (work.LicenseCheckFuture.valid() &&
                    work.LicenseCheckFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const SdkLicenseQuery query = work.LicenseCheckFuture.get();
                    work.LicenseBusy = false;
                    if (query.Status == LicenseStatus::AllAccepted) {
                        StartInstall(context, work.PendingPackagePath);
                        work.PendingPackagePath.clear();
                    } else if (query.Status == LicenseStatus::SomeUnaccepted) {
                        work.AwaitingLicenseConsent = true;
                    } else {
                        work.LicenseError = Tr("Could not query license state. Check that the SDK Manager is working.");
                        if (!query.FailureDetail.empty()) {
                            work.LicenseError.push_back('\n');
                            work.LicenseError += query.FailureDetail;
                        }
                        work.PendingPackagePath.clear();
                    }
                }

                if (work.LicenseAcceptFuture.valid() &&
                    work.LicenseAcceptFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const bool ok = work.LicenseAcceptFuture.get();
                    work.LicenseBusy = false;
                    work.AwaitingLicenseConsent = false;
                    if (ok && !work.PendingPackagePath.empty()) {
                        StartInstall(context, work.PendingPackagePath);
                        work.PendingPackagePath.clear();
                    } else {
                        work.LicenseError = Tr("License acceptance failed. Try again or accept via Android Studio.");
                        work.PendingPackagePath.clear();
                    }
                }

                if (work.AwaitingLicenseConsent) {
                    const bool licenseBusy = work.LicenseBusy.load();

                    ImGui::Text("%s", Tr("Accept Android SDK License Terms"));
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "%s",
                        Tr(
                            "Some Android SDK package licenses have not been accepted yet. "
                            "To install this system image, you must agree to Google's Android "
                            "SDK license terms. By clicking Agree, you confirm that you have "
                            "read and accept the current terms."
                        )
                    );
                    ImGui::Spacing();
                    if (ImGui::TextLink(Tr("View License Terms..."))) {
                        OpenUrl(SDK_LICENSE_URL);
                    }
                    if (licenseBusy) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("%s", Tr("Recording acceptance with the SDK Manager..."));
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    const float halfWidth2 = EqualButtonWidth(2);

                    if (PositiveButton(Tr("Agree & Install"), !licenseBusy, ImVec2(halfWidth2, 0))) {
                        work.LicenseBusy = true;
                        const SdkInfo sdk = context.Host.Sdk;
                        work.LicenseAcceptFuture = std::async(std::launch::async, [sdk] {
                            return AcceptSdkLicenses(sdk);
                        });
                    }
                    ImGui::SameLine();
                    if (NegativeButton(Tr("Cancel"), !licenseBusy, ImVec2(halfWidth2, 0))) {
                        work.AwaitingLicenseConsent = false;
                        work.PendingPackagePath.clear();
                    }

                    ImGui::EndPopup();
                    return;
                }

                if (!isInstalling && work.InstallFuture.valid()) {
                    if (work.InstallFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        if (work.InstallFuture.get()) {
                            RefreshSystemImageLists(context);
                        }
                    }
                }

                if (removalBusy && removal.Future.valid()) {
                    if (removal.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        const bool removed = removal.Future.get();
                        removal.Busy = false;
                        if (removed) {
                            RefreshSystemImageLists(context);
                        }
                    }
                }

                if (isInstalling || removalBusy) {
                    ImGui::BeginDisabled();
                }

                SearchField("##RemoteImageSearch", Tr("Search for a system image by name"), work.SearchFilter, sizeof(work.SearchFilter));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("Categories"));

                const char *categoryLabels[] = {
                    Tr("All"),
                    Tr("Phone / Tablet"),
                    Tr("Wear OS"),
                    Tr("TV"),
                    Tr("Automotive"),
                    Tr("Desktop"),
                    Tr("XR"),
                    Tr("Other"),
                };
                static_assert(IM_ARRAYSIZE(categoryLabels) == static_cast<int>(ImageCategory::Other) + 1);
                int selectedCategory = static_cast<int>(work.SelectedCategory);
                if (CategoryChipRow(categoryLabels, IM_ARRAYSIZE(categoryLabels), selectedCategory)) {
                    work.SelectedCategory = static_cast<ImageCategory>(selectedCategory);
                    work.SelectedImage = -1;
                }

                ImGui::Spacing();
                ImGui::Text("%s", Tr("Available System Images"));
                if (isLoading) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", Tr("Fetching available images from SDK manager..."));
                }
                ImGui::Spacing();

                int visibleCount = 0;
                {
                    PickerTableStyle pts;
                    const bool tableOpen = BeginPickerTable("##RemoteImageTableFrame", "##RemoteImageTable", 6, Eh(14.0F));
                    if (tableOpen) {
                        const std::string nameColumn = StrConcat(" ", Tr("Name"));
                        ImGui::TableSetupColumn(nameColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 2.4F);
                        ImGui::TableSetupColumn(Tr("Type"), ImGuiTableColumnFlags_WidthStretch, 1.5F);
                        ImGui::TableSetupColumn(Tr("API"), ImGuiTableColumnFlags_WidthStretch, 1.2F);
                        ImGui::TableSetupColumn(Tr("ABI"), ImGuiTableColumnFlags_WidthStretch, 1.3F);
                        ImGui::TableSetupColumn(Tr("Page Size"), ImGuiTableColumnFlags_WidthFixed, Em(10.0F));
                        ImGui::TableSetupColumn(Tr("Status"), ImGuiTableColumnFlags_WidthStretch, 1.2F);
                        ImGui::TableHeadersRow();

                        if (isLoading || !work.RemoteImages.empty()) {
                            for (int i = 0; i < static_cast<int>(work.RemoteImages.size()); i++) {
                                const auto &img = work.RemoteImages.at(static_cast<std::size_t>(i));
                                if (!MatchesImageFilters(img, work.SearchFilter, work.SelectedCategory)) {
                                    continue;
                                }

                                visibleCount++;
                                const bool isSelected = work.SelectedImage == i;
                                const auto [_, slabel, color] = SystemImageTypeStyleFor(img);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                const std::string label = StrConcat(
                                    " ",
                                    SystemImageDisplayName(img.ApiLevel, img.DisplayName),
                                    "##RemoteImage",
                                    std::to_string(i)
                                );
                                if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0F, 0.0F))) {
                                    work.SelectedImage = i;
                                }
                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }

                                ImGui::TableNextColumn();
                                ImGui::TextColored(HexColor(color), "%s", slabel);

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", img.ApiLevel.c_str());

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", img.Abi.c_str());

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", SystemImagePageSizeLabel(img.Variant));

                                ImGui::TableNextColumn();
                                if (img.IsInstalled) {
                                    ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", Tr("Installed"));
                                } else {
                                    ImGui::TextDisabled("%s", Tr("Available"));
                                }
                            }
                        }
                    }
                    EndPickerTable(tableOpen);
                }

                if (!isLoading && work.RemoteImages.empty()) {
                    ImGui::Spacing();
                    StatusMessage(
                        StatusMessageTone::Error,
                        Tr("No remote system images found. Check your SDK and internet connection.")
                    );
                } else if (!isLoading && !work.RemoteImages.empty() && visibleCount == 0) {
                    ImGui::Spacing();
                    StatusMessage(StatusMessageTone::Info, Tr("No system images available."));
                }

                if (isInstalling || removalBusy) {
                    ImGui::EndDisabled();
                }

                if (isInstalling && work.Progress) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    float fraction = 0.0F;
                    std::string statusText;
                    {
                        std::scoped_lock lock(work.Progress->Mutex);
                        fraction = work.Progress->Percent;
                        statusText = work.Progress->StatusText;
                    }

                    const TaskProgress task{
                        .Subtitle = statusText.c_str(),
                        .Fraction = fraction,
                        .CenterHorizontally = false,
                    };
                    TaskProgressPanel(task);
                }

                if (!isInstalling && work.Progress) {
                    bool finished = false;
                    bool succeeded = false;
                    std::string statusText;
                    {
                        std::scoped_lock lock(work.Progress->Mutex);
                        finished = work.Progress->Finished;
                        succeeded = work.Progress->Succeeded;
                        statusText = work.Progress->StatusText;
                    }

                    if (finished && !statusText.empty()) {
                        ImGui::Spacing();
                        StatusMessage(
                            succeeded ? StatusMessageTone::Positive : StatusMessageTone::Error,
                            statusText.c_str()
                        );
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const bool hasVisibleSelection =
                    work.SelectedImage >= 0 &&
                    work.SelectedImage < static_cast<int>(work.RemoteImages.size()) &&
                    MatchesImageFilters(work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage)), work.SearchFilter, work.SelectedCategory);

                const bool selectedInstalled = hasVisibleSelection && work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage)).IsInstalled;
                const bool canUseSelected = !isLoading && !isInstalling && !removalBusy && selectedInstalled;
                const bool canRemove = canUseSelected;
                const bool canInstall = !isLoading && !isInstalling && !removalBusy && hasVisibleSelection && !selectedInstalled;

                const float halfWidth = EqualButtonWidth(2);
                const float thirdWidth = EqualButtonWidth(3);

                const bool licenseBusy = work.LicenseBusy.load();

                if (!work.LicenseError.empty()) {
                    StatusMessage(StatusMessageTone::Error, work.LicenseError.c_str());
                    ImGui::Spacing();
                }

                if (selectedInstalled || removalBusy) {
                    if (removalBusy) {
                        ImGui::BeginDisabled();
                        PositiveButton(Tr("Use Selected Image"), false, ImVec2(thirdWidth, 0));
                        ImGui::EndDisabled();
                    } else if (PositiveButton(Tr("Use Selected Image"), canUseSelected, ImVec2(thirdWidth, 0))) {
                        const auto &img = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage));
                        if (SelectInstalledSystemImage(context, img.PackagePath)) {
                            work.Progress.reset();
                            context.UI.ShowInstallImageDialog = false;
                        }
                    }

                    ImGui::SameLine();
                    if (removalBusy) {
                        NegativeButton(Tr("Removing..."), true, ImVec2(thirdWidth, 0), true);
                    } else if (NegativeButton(Tr("Remove Image"), canRemove, ImVec2(thirdWidth, 0))) {
                        const std::string pkg = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage)).PackagePath;
                        removal.Busy = true;
                        const SdkInfo sdk = context.Host.Sdk;
                        removal.Future = std::async(std::launch::async, [sdk, pkg] {
                            try {
                                return UninstallSystemImage(sdk, pkg);
                            } catch (...) {
                                return false;
                            }
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton(Tr("Close"), !isInstalling && !removalBusy, ImVec2(thirdWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (isInstalling) {
                    PositiveButton(Tr("Installing..."), true, ImVec2(halfWidth, 0), true);
                    ImGui::SameLine();
                    if (PrimaryButton(Tr("Close"), false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (licenseBusy) {
                    PositiveButton(Tr("Checking licenses..."), true, ImVec2(halfWidth, 0), true);
                    ImGui::SameLine();
                    if (PrimaryButton(Tr("Close"), false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else {
                    if (PositiveButton(Tr("Install"), canInstall, ImVec2(halfWidth, 0))) {
                        const auto &img = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage));
                        work.PendingPackagePath = img.PackagePath;
                        work.LicenseError.clear();
                        work.LicenseBusy = true;
                        const SdkInfo sdk = context.Host.Sdk;
                        work.LicenseCheckFuture = std::async(std::launch::async, [sdk] {
                            return QuerySdkLicenses(sdk);
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton(Tr("Close"), !isInstalling && !removalBusy, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                }

                ImGui::EndPopup();
            }
        }
    }
}

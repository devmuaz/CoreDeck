//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include "imgui.h"

#include "install_image.h"
#include "../widgets.h"
#include "../theme.h"
#include "core/sdk.h"
#include "../../core/sdk_manager.h"
#include "platform/utilities.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        ImageCategory CategoryForImage(const Core::RemoteSystemImage &img) {
            const std::string searchable = Platform::LowerCopy(Platform::StrConcat(img.PackagePath, " ", img.Variant, " ", img.DisplayName));

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

        bool MatchesImageCategory(const Core::RemoteSystemImage &img, const ImageCategory category) {
            return category == ImageCategory::All || CategoryForImage(img) == category;
        }

        const char *SystemImagePageSizeLabel(const std::string &variant) {
            return variant.find("ps16k") != std::string::npos ? Platform::Tr("16 KB") : Platform::Tr("4 KB");
        }

        bool MatchesImageFilter(const Core::RemoteSystemImage &img, const char *filter) {
            if (!filter || filter[0] == '\0') {
                return true;
            }

            const auto searchable = Platform::StrConcat(
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
            return Platform::ContainsIgnoreCase(searchable, filter);
        }

        bool MatchesImageFilters(const Core::RemoteSystemImage &img, const char *filter, const ImageCategory category) {
            return MatchesImageCategory(img, category) && MatchesImageFilter(img, filter);
        }

        void StartInstall(Context &context, const std::string &pkgPath) {
            auto &work = context.ImageInstallationWork;
            work.Progress = std::make_shared<Core::InstallProgressData>();
            work.Installing = true;
            const auto progress = work.Progress;
            const Core::SdkInfo sdk = context.Host.Sdk;
            work.InstallFuture = std::async(
                std::launch::async,
                [&context, sdk, pkgPath, progress] {
                    const bool ok = Core::InstallSystemImage(sdk, pkgPath, progress);
                    context.ImageInstallationWork.Installing = false;
                    return ok;
                }
            );
        }

        bool SelectInstalledSystemImage(Context &context, const std::string &packagePath) {
            auto &images = context.AvdCreationWork.SystemImages;
            for (int i = 0; std::cmp_less(i, images.size()); i++) {
                if (images.at(static_cast<std::size_t>(i)).PackagePath == packagePath) {
                    context.AvdCreationWork.SelectedSystemImage = i;
                    return true;
                }
            }

            images = Core::ListSystemImages(context.Host.Sdk);
            for (int i = 0; std::cmp_less(i, images.size()); i++) {
                if (images.at(static_cast<std::size_t>(i)).PackagePath == packagePath) {
                    context.AvdCreationWork.SelectedSystemImage = i;
                    return true;
                }
            }

            return false;
        }

        void RefreshSystemImageLists(Context &context) {
            auto &images = context.AvdCreationWork.SystemImages;
            images = Core::ListSystemImages(context.Host.Sdk);
            if (images.empty()) {
                context.AvdCreationWork.SelectedSystemImage = 0;
            } else {
                context.AvdCreationWork.SelectedSystemImage = std::clamp(
                    context.AvdCreationWork.SelectedSystemImage,
                    0,
                    static_cast<int>(images.size()) - 1
                );
            }

            context.ImageInstallationWork.RemoteImages = Core::ListRemoteSystemImages(context.Host.Sdk, images);
        }

        struct VisibleImageChoice {
            bool Visible = false;
            bool Installed = false;
        };

        VisibleImageChoice CurrentVisibleImage(const Context &context) {
            const auto &work = context.ImageInstallationWork;
            VisibleImageChoice choice;
            if (work.SelectedImage < 0 || std::cmp_greater_equal(work.SelectedImage, work.RemoteImages.size())) {
                return choice;
            }
            const auto &image = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage));
            if (!MatchesImageFilters(image, work.SearchFilter, work.SelectedCategory)) {
                return choice;
            }
            choice.Visible = true;
            choice.Installed = image.IsInstalled;
            return choice;
        }

        int CountVisibleImages(const Context &context) {
            const auto &work = context.ImageInstallationWork;
            int count = 0;
            for (const auto &image: work.RemoteImages) {
                if (MatchesImageFilters(image, work.SearchFilter, work.SelectedCategory)) {
                    ++count;
                }
            }
            return count;
        }

        // The bar shows exactly one of these. Several inputs can be true at once;
        // ResolveInstallImageStatus picks the one that wins.
        enum class InstallImageStatus : uint8_t {
            LicenseError,
            Installing,
            InstallSucceeded,
            InstallFailed,
            Removing,
            CheckingLicenses,
            Loading,
            NoRemoteImages,
            NoMatches,
            Installed,
            ReadyToInstall,
            SelectImage,
        };

        struct InstallImageStatusView {
            InstallImageStatus Status = InstallImageStatus::SelectImage;
            std::string Detail;
        };

        InstallImageStatusView ResolveInstallImageStatus(const Context &context) {
            const auto &work = context.ImageInstallationWork;
            const bool isLoading = !work.Prefetch.Ready.load();
            const bool isInstalling = work.Installing.load();
            const bool removalBusy = context.AvdCreationWork.SystemImageRemoval.Busy.load();
            const bool licenseBusy = work.LicenseBusy.load();
            const VisibleImageChoice selection = CurrentVisibleImage(context);

            if (!work.LicenseError.empty()) {
                return {.Status = InstallImageStatus::LicenseError, .Detail = work.LicenseError};
            }

            if (work.Progress) {
                bool finished = false;
                bool succeeded = false;
                std::string statusText;
                {
                    const std::scoped_lock lock(work.Progress->Mutex);
                    finished = work.Progress->Finished;
                    succeeded = work.Progress->Succeeded;
                    statusText = work.Progress->StatusText;
                }
                if (isInstalling) {
                    return {.Status = InstallImageStatus::Installing, .Detail = std::move(statusText)};
                }
                if (finished && !statusText.empty()) {
                    return {
                        .Status = succeeded ? InstallImageStatus::InstallSucceeded : InstallImageStatus::InstallFailed,
                        .Detail = std::move(statusText),
                    };
                }
            }

            if (removalBusy) {
                return {.Status = InstallImageStatus::Removing};
            }
            if (licenseBusy) {
                return {.Status = InstallImageStatus::CheckingLicenses};
            }
            if (isLoading) {
                return {.Status = InstallImageStatus::Loading};
            }
            if (work.RemoteImages.empty()) {
                return {.Status = InstallImageStatus::NoRemoteImages};
            }
            if (CountVisibleImages(context) == 0) {
                return {.Status = InstallImageStatus::NoMatches};
            }
            if (selection.Visible && selection.Installed) {
                return {.Status = InstallImageStatus::Installed};
            }
            if (selection.Visible) {
                return {.Status = InstallImageStatus::ReadyToInstall};
            }
            return {.Status = InstallImageStatus::SelectImage};
        }

        void BuildInstallImageStatusBar(const Context &context) {
            const InstallImageStatusView status = ResolveInstallImageStatus(context);
            switch (status.Status) {
                case InstallImageStatus::LicenseError:
                    StatusBar(StatusBarTone::Error, status.Detail.c_str());
                    return;
                case InstallImageStatus::Installing:
                    StatusBar(
                        StatusBarTone::Busy,
                        status.Detail.empty() ? Platform::Tr("Installing the system image.") : status.Detail.c_str()
                    );
                    return;
                case InstallImageStatus::InstallSucceeded:
                    StatusBar(StatusBarTone::Success, status.Detail.c_str());
                    return;
                case InstallImageStatus::InstallFailed:
                    StatusBar(StatusBarTone::Error, status.Detail.c_str());
                    return;
                case InstallImageStatus::Removing:
                    StatusBar(StatusBarTone::Busy, Platform::Tr("Removing the system image."));
                    return;
                case InstallImageStatus::CheckingLicenses:
                    StatusBar(StatusBarTone::Busy, Platform::Tr("Checking licenses..."));
                    return;
                case InstallImageStatus::Loading:
                    StatusBar(StatusBarTone::Busy, Platform::Tr("Loading..."));
                    return;
                case InstallImageStatus::NoRemoteImages:
                    StatusBar(
                        StatusBarTone::Error,
                        Platform::Tr("No remote system images found. Check your SDK and internet connection.")
                    );
                    return;
                case InstallImageStatus::NoMatches:
                    StatusBar(StatusBarTone::Idle, Platform::Tr("No system images available."));
                    return;
                case InstallImageStatus::Installed:
                    StatusBar(StatusBarTone::Idle, Platform::Tr("This system image is installed."));
                    return;
                case InstallImageStatus::ReadyToInstall:
                    StatusBar(StatusBarTone::Idle, Platform::Tr("Ready to install this system image."));
                    return;
                case InstallImageStatus::SelectImage:
                    StatusBar(StatusBarTone::Idle, Platform::Tr("Select a system image to install."));
                    return;
            }
        }
    }

    LabeledIconStyle SystemImageTypeStyleForVariant(const std::string &variant) {
        if (variant.starts_with("google_apis_playstore")) {
            return {.Icon = Icons::PLAY, .Label = Platform::Tr("Google Play"), .Color = Colors::POSITIVE};
        }
        if (variant.starts_with("google_apis")) {
            return {.Icon = Icons::GEAR, .Label = Platform::Tr("Google APIs"), .Color = Colors::ACCENT_PHONE};
        }
        if (variant.starts_with("aosp_atd") || variant.starts_with("google_atd")) {
            return {.Icon = Icons::MOBILE, .Label = "ATD", .Color = Colors::ACCENT_WEAR};
        }
        return {.Icon = Icons::MOBILE, .Label = Platform::Tr("Default"), .Color = Colors::TEXT_SUBTLE};
    }

    LabeledIconStyle SystemImageTypeStyleFor(const Core::SystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    LabeledIconStyle SystemImageTypeStyleFor(const Core::RemoteSystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    std::string SystemImageDisplayName(const std::string &apiLevel, const std::string &fallback) {
        if (!apiLevel.empty()) {
            return Platform::TrFormat("Android {0}", apiLevel);
        }
        return fallback;
    }

    std::string SystemImagePreviewLabel(const Core::SystemImage &img) {
        const auto style = SystemImageTypeStyleFor(img);
        return Platform::StrConcat(
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

        const Core::SdkInfo sdk = context.Host.Sdk;
        context.ImageInstallationWork.Prefetch.Future = std::async(std::launch::async, [&context, sdk] {
            const auto localImages = Core::ListSystemImages(sdk);
            auto remoteImages = Core::ListRemoteSystemImages(sdk, localImages);
            context.AvdCreationWork.SystemImages = localImages;
            context.ImageInstallationWork.RemoteImages = std::move(remoteImages);
            context.ImageInstallationWork.Prefetch.Loading = false;
            context.ImageInstallationWork.Prefetch.Ready = true;
        });
    }

    // NOLINTNEXTLINE(readability-function-size)
    void BuildInstallImageWindow(Context &context) {
        if (context.UI.ShowInstallImageDialog) {
            const std::string title = Platform::TrWindow("Install System Image", "InstallImageDialog");
            const bool installing = context.ImageInstallationWork.Installing.load();
            const bool removalBusy = context.AvdCreationWork.SystemImageRemoval.Busy.load();
            bool *pOpen = (installing || removalBusy) ? nullptr : &context.UI.ShowInstallImageDialog;

            if (BeginCenteredModal(title.c_str(), pOpen, EmV(100.0F, 28.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
                auto &work = context.ImageInstallationWork;
                auto &removal = context.AvdCreationWork.SystemImageRemoval;
                const bool isInstalling = installing;

                if (work.LicenseCheckFuture.valid() &&
                    work.LicenseCheckFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const Core::SdkLicenseQuery query = work.LicenseCheckFuture.get();
                    work.LicenseBusy = false;
                    if (query.Status == Core::LicenseStatus::AllAccepted) {
                        StartInstall(context, work.PendingPackagePath);
                        work.PendingPackagePath.clear();
                    } else if (query.Status == Core::LicenseStatus::SomeUnaccepted) {
                        work.AwaitingLicenseConsent = true;
                    } else {
                        work.LicenseError = Platform::Tr("Could not query license state. Check that the SDK Manager is working.");
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
                        work.LicenseError = Platform::Tr("License acceptance failed. Try again or accept via Android Studio.");
                        work.PendingPackagePath.clear();
                    }
                }

                if (work.AwaitingLicenseConsent) {
                    const bool licenseBusy = work.LicenseBusy.load();

                    ImGui::Text("%s", Platform::Tr("Accept Android SDK License Terms"));
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "%s",
                        Platform::Tr(
                            "Some Android SDK package licenses have not been accepted yet. "
                            "To install this system image, you must agree to Google's Android "
                            "SDK license terms. By clicking Agree, you confirm that you have "
                            "read and accept the current terms."
                        )
                    );
                    ImGui::Spacing();
                    if (ImGui::TextLink(Platform::Tr("View License Terms..."))) {
                        Platform::OpenUrl(Core::SDK_LICENSE_URL);
                    }
                    if (licenseBusy) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("%s", Platform::Tr("Recording acceptance with the SDK Manager..."));
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    const float halfWidth2 = EqualButtonWidth(2);

                    if (PositiveButton(Platform::Tr("Agree & Install"), !licenseBusy, ImVec2(halfWidth2, 0))) {
                        work.LicenseBusy = true;
                        const Core::SdkInfo sdk = context.Host.Sdk;
                        work.LicenseAcceptFuture = std::async(std::launch::async, [sdk] {
                            return Core::AcceptSdkLicenses(sdk);
                        });
                    }
                    ImGui::SameLine();
                    if (NegativeButton(Platform::Tr("Cancel"), !licenseBusy, ImVec2(halfWidth2, 0))) {
                        work.AwaitingLicenseConsent = false;
                        work.PendingPackagePath.clear();
                    }

                    ImGui::EndPopup();
                    return;
                }

                const bool isLoading = !work.Prefetch.Ready.load();

                if (!isInstalling && work.InstallFuture.valid() && work.InstallFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready && work.InstallFuture.get())

                {
                    RefreshSystemImageLists(context);
                }


                if ((removalBusy && removal.Future.valid()) && (removal.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)) {
                    const bool removed = removal.Future.get();
                    removal.Busy = false;
                    if (removed) {
                        RefreshSystemImageLists(context);
                    }
                }


                if (isInstalling || removalBusy) {
                    ImGui::BeginDisabled();
                }

                SearchField("##RemoteImageSearch", Platform::Tr("Search for a system image by name"), work.SearchFilter, sizeof(work.SearchFilter));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", Platform::Tr("Categories"));

                const char *categoryLabels[] = {
                    Platform::Tr("All"),
                    Platform::Tr("Phone / Tablet"),
                    Platform::Tr("Wear OS"),
                    Platform::Tr("TV"),
                    Platform::Tr("Automotive"),
                    Platform::Tr("Desktop"),
                    Platform::Tr("XR"),
                    Platform::Tr("Other"),
                };
                static_assert(IM_ARRAYSIZE(categoryLabels) == static_cast<int>(ImageCategory::Other) + 1);
                int selectedCategory = static_cast<int>(work.SelectedCategory);
                if (CategoryChipRow(categoryLabels, IM_ARRAYSIZE(categoryLabels), selectedCategory)) {
                    work.SelectedCategory = static_cast<ImageCategory>(selectedCategory);
                    work.SelectedImage = -1;
                }

                ImGui::Spacing();
                ImGui::Text("%s", Platform::Tr("Available System Images"));
                ImGui::Spacing();

                {
                    const PickerTableStyle pts;
                    const bool tableOpen = BeginPickerTable("##RemoteImageTableFrame", "##RemoteImageTable", 6, Eh(14.0F));
                    if (tableOpen) {
                        const std::string nameColumn = Platform::StrConcat(" ", Platform::Tr("Name"));
                        ImGui::TableSetupColumn(nameColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 2.4F);
                        ImGui::TableSetupColumn(Platform::Tr("Type"), ImGuiTableColumnFlags_WidthStretch, 1.5F);
                        ImGui::TableSetupColumn(Platform::Tr("API"), ImGuiTableColumnFlags_WidthStretch, 1.2F);
                        ImGui::TableSetupColumn(Platform::Tr("ABI"), ImGuiTableColumnFlags_WidthStretch, 1.3F);
                        ImGui::TableSetupColumn(Platform::Tr("Page Size"), ImGuiTableColumnFlags_WidthFixed, Em(10.0F));
                        ImGui::TableSetupColumn(Platform::Tr("Status"), ImGuiTableColumnFlags_WidthStretch, 1.2F);
                        ImGui::TableHeadersRow();

                        if (!work.RemoteImages.empty()) {
                            for (int i = 0; std::cmp_less(i, work.RemoteImages.size()); i++) {
                                const auto &img = work.RemoteImages.at(static_cast<std::size_t>(i));
                                if (!MatchesImageFilters(img, work.SearchFilter, work.SelectedCategory)) {
                                    continue;
                                }

                                const bool isSelected = work.SelectedImage == i;
                                const auto [_, slabel, color] = SystemImageTypeStyleFor(img);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                const std::string label = Platform::StrConcat(
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
                                    ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", Platform::Tr("Installed"));
                                } else {
                                    ImGui::TextDisabled("%s", Platform::Tr("Available"));
                                }
                            }
                        }
                    }
                    EndPickerTable(tableOpen);
                }

                if (isInstalling || removalBusy) {
                    ImGui::EndDisabled();
                }

                if (isInstalling && work.Progress) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    float fraction = 0.0F;
                    {
                        const std::scoped_lock lock(work.Progress->Mutex);
                        fraction = work.Progress->Percent;
                    }

                    const TaskProgress task{
                        .Fraction = fraction,
                        .CenterHorizontally = false,
                    };
                    TaskProgressPanel(task);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const VisibleImageChoice selection = CurrentVisibleImage(context);
                const bool hasVisibleSelection = selection.Visible;
                const bool selectedInstalled = selection.Installed;
                const bool canUseSelected = !isLoading && !isInstalling && !removalBusy && selectedInstalled;
                const bool canRemove = canUseSelected;
                const bool canInstall = !isLoading && !isInstalling && !removalBusy && hasVisibleSelection && !selectedInstalled;

                const float halfWidth = EqualButtonWidth(2);
                const float thirdWidth = EqualButtonWidth(3);

                const bool licenseBusy = work.LicenseBusy.load();

                if (selectedInstalled || removalBusy) {
                    if (removalBusy) {
                        ImGui::BeginDisabled();
                        PositiveButton(Platform::Tr("Use Selected Image"), false, ImVec2(thirdWidth, 0));
                        ImGui::EndDisabled();
                    } else if (PositiveButton(Platform::Tr("Use Selected Image"), canUseSelected, ImVec2(thirdWidth, 0))) {
                        const auto &img = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage));
                        if (SelectInstalledSystemImage(context, img.PackagePath)) {
                            work.Progress.reset();
                            context.UI.ShowInstallImageDialog = false;
                        }
                    }

                    ImGui::SameLine();
                    if (removalBusy) {
                        NegativeButton(Platform::Tr("Removing..."), true, ImVec2(thirdWidth, 0), true);
                    } else if (NegativeButton(Platform::Tr("Remove Image"), canRemove, ImVec2(thirdWidth, 0))) {
                        const std::string pkg = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage)).PackagePath;
                        removal.Busy = true;
                        const Core::SdkInfo sdk = context.Host.Sdk;
                        removal.Future = std::async(std::launch::async, [sdk, pkg] {
                            try {
                                return Core::UninstallSystemImage(sdk, pkg);
                            } catch (...) {
                                return false;
                            }
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton(Platform::Tr("Close"), !isInstalling && !removalBusy, ImVec2(thirdWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (isInstalling) {
                    PositiveButton(Platform::Tr("Installing..."), true, ImVec2(halfWidth, 0), true);
                    ImGui::SameLine();
                    if (PrimaryButton(Platform::Tr("Close"), false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (licenseBusy) {
                    PositiveButton(Platform::Tr("Checking licenses..."), true, ImVec2(halfWidth, 0), true);
                    ImGui::SameLine();
                    if (PrimaryButton(Platform::Tr("Close"), false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else {
                    if (PositiveButton(Platform::Tr("Install"), canInstall, ImVec2(halfWidth, 0))) {
                        const auto &img = work.RemoteImages.at(static_cast<std::size_t>(work.SelectedImage));
                        work.PendingPackagePath = img.PackagePath;
                        work.LicenseError.clear();
                        work.LicenseBusy = true;
                        const Core::SdkInfo sdk = context.Host.Sdk;
                        work.LicenseCheckFuture = std::async(std::launch::async, [sdk] {
                            return Core::QuerySdkLicenses(sdk);
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton(Platform::Tr("Close"), !isInstalling && !removalBusy, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                }

                ImGui::Spacing();
                BuildInstallImageStatusBar(context);

                ImGui::EndPopup();
            }
        }
    }
}

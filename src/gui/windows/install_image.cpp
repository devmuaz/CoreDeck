//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <chrono>
#include <string>
#include "imgui.h"

#include "install_image.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    struct ImageCategoryOption {
        ImageCategory Category;
        const char *Label;
    };

    static ImageCategory CategoryForImage(const RemoteSystemImage &img) {
        const std::string searchable = LowerCopy(StrConcat(img.PackagePath, " ", img.Variant, " ", img.DisplayName));

        if (searchable.find("wear") != std::string::npos) return ImageCategory::Wear;
        if (searchable.find("automotive") != std::string::npos || searchable.find("android-auto") != std::string::npos) {
            return ImageCategory::Automotive;
        }
        if (searchable.find("desktop") != std::string::npos) return ImageCategory::Desktop;
        if (searchable.find("xr") != std::string::npos) return ImageCategory::Xr;
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

    static bool MatchesImageCategory(const RemoteSystemImage &img, const ImageCategory category) {
        return category == ImageCategory::All || CategoryForImage(img) == category;
    }

    static bool MatchesImageFilter(const RemoteSystemImage &img, const char *filter) {
        if (!filter || filter[0] == '\0') return true;

        const auto searchable = StrConcat(img.DisplayName, " ", img.ApiLevel, " ", img.Variant, " ", img.Abi, " ", img.PackagePath);
        return ContainsIgnoreCase(searchable, filter);
    }

    static bool MatchesImageFilters(const RemoteSystemImage &img, const char *filter, const ImageCategory category) {
        return MatchesImageCategory(img, category) && MatchesImageFilter(img, filter);
    }

    static void StartInstall(Context &context, const std::string &pkgPath) {
        auto &work = context.ImageInstallationWork;
        work.Progress = std::make_shared<InstallProgressData>();
        work.Installing = true;
        auto progress = work.Progress;
        work.InstallFuture = std::async(
            std::launch::async,
            [&context, pkgPath, progress] {
                const bool ok = InstallSystemImage(context.Host.Sdk, pkgPath, progress);
                context.ImageInstallationWork.Installing = false;
                return ok;
            }
        );
    }

    static bool SelectInstalledSystemImage(Context &context, const std::string &packagePath) {
        auto &images = context.AvdCreationWork.SystemImages;
        for (int i = 0; i < static_cast<int>(images.size()); i++) {
            if (images[i].PackagePath == packagePath) {
                context.AvdCreationWork.SelectedSystemImage = i;
                return true;
            }
        }

        images = ListSystemImages(context.Host.Sdk);
        for (int i = 0; i < static_cast<int>(images.size()); i++) {
            if (images[i].PackagePath == packagePath) {
                context.AvdCreationWork.SelectedSystemImage = i;
                return true;
            }
        }

        return false;
    }

    static void RefreshSystemImageLists(Context &context) {
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

    LabeledIconStyle SystemImageTypeStyleForVariant(const std::string &variant) {
        if (variant.starts_with("google_apis_playstore")) return {Icons::Play, "Google Play", Colors::Positive};
        if (variant.starts_with("google_apis")) return {Icons::Gear, "Google APIs", Colors::AccentPhone};
        if (variant.starts_with("aosp_atd") || variant.starts_with("google_atd")) {
            return {Icons::Mobile, "ATD", Colors::AccentWear};
        }
        return {Icons::Mobile, "Default", Colors::TextSubtle};
    }

    LabeledIconStyle SystemImageTypeStyleFor(const SystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    LabeledIconStyle SystemImageTypeStyleFor(const RemoteSystemImage &img) {
        return SystemImageTypeStyleForVariant(img.Variant);
    }

    std::string SystemImageDisplayName(const std::string &apiLevel, const std::string &fallback) {
        if (!apiLevel.empty()) return StrConcat("Android ", apiLevel);
        return fallback;
    }

    std::string SystemImagePreviewLabel(const SystemImage &img) {
        const auto style = SystemImageTypeStyleFor(img);
        return StrConcat(SystemImageDisplayName(img.ApiLevel, img.DisplayName), " - ", style.Label, " - ", img.Abi);
    }

    void BuildInstallImageWindow(Context &context) {
        if (context.UI.ShowInstallImageDialog) {
            constexpr auto title = "Install System Image###InstallImageDialog";
            if (!ImGui::IsPopupOpen(title)) {
                ImGui::OpenPopup(title);
            }

            const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_Appearing);

            const bool installing = context.ImageInstallationWork.Installing.load();
            const bool removalBusy = context.AvdCreationWork.SystemImageRemoval.Busy.load();
            bool *pOpen = (installing || removalBusy) ? nullptr : &context.UI.ShowInstallImageDialog;

            if (ImGui::BeginPopupModal(title, pOpen, WindowAutoResizeFlags)) {
                auto &work = context.ImageInstallationWork;
                auto &removal = context.AvdCreationWork.SystemImageRemoval;
                const bool isLoading = work.Prefetch.Loading.load();
                const bool isInstalling = installing;

                if (work.LicenseCheckFuture.valid() &&
                    work.LicenseCheckFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const LicenseStatus status = work.LicenseCheckFuture.get();
                    work.LicenseBusy = false;
                    if (status == LicenseStatus::AllAccepted) {
                        StartInstall(context, work.PendingPackagePath);
                        work.PendingPackagePath.clear();
                    } else if (status == LicenseStatus::SomeUnaccepted) {
                        work.AwaitingLicenseConsent = true;
                    } else {
                        work.LicenseError = "Could not query license state. Check that the SDK Manager is working.";
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
                        work.LicenseError = "License acceptance failed. Try again or accept via Android Studio.";
                        work.PendingPackagePath.clear();
                    }
                }

                if (work.AwaitingLicenseConsent) {
                    const bool licenseBusy = work.LicenseBusy.load();

                    ImGui::Text("Accept Android SDK License Terms");
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "Some Android SDK package licenses have not been accepted yet. "
                        "To install this system image, you must agree to Google's Android "
                        "SDK license terms. By clicking Agree, you confirm that you have "
                        "read and accept the current terms."
                    );
                    ImGui::Spacing();
                    if (PrimaryButton("Open license terms in browser")) {
                        OpenUrl("https://developer.android.com/studio/terms");
                    }

                    if (licenseBusy) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Recording acceptance with the SDK Manager...");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    const float spacing2 = ImGui::GetStyle().ItemSpacing.x;
                    const float halfWidth2 = (ImGui::GetContentRegionAvail().x - spacing2) * 0.5f;

                    if (PositiveButton("Agree & Install", !licenseBusy, ImVec2(halfWidth2, 0))) {
                        work.LicenseBusy = true;
                        work.LicenseAcceptFuture = std::async(std::launch::async, [&context] {
                            return AcceptSdkLicenses(context.Host.Sdk);
                        });
                    }
                    ImGui::SameLine();
                    if (NegativeButton("Cancel", !licenseBusy, ImVec2(halfWidth2, 0))) {
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
                        if (removed) RefreshSystemImageLists(context);
                    }
                }

                if (isInstalling || removalBusy) ImGui::BeginDisabled();

                ImGui::SetNextItemWidth(-1.0f);
                const std::string searchHint = IconWithLabel(Icons::Search, "Search for a system image by name");
                ImGui::InputTextWithHint("##RemoteImageSearch", searchHint.c_str(), work.SearchFilter, sizeof(work.SearchFilter));

                ImGui::Spacing();
                ImGui::TextDisabled("Categories");

                static constexpr ImageCategoryOption categoryOptions[] = {
                    {ImageCategory::All, "All"},
                    {ImageCategory::PhoneTablet, "Phone / Tablet"},
                    {ImageCategory::Wear, "Wear OS"},
                    {ImageCategory::Tv, "TV"},
                    {ImageCategory::Automotive, "Automotive"},
                    {ImageCategory::Desktop, "Desktop"},
                    {ImageCategory::Xr, "XR"},
                    {ImageCategory::Other, "Other"},
                };

                bool firstCategory = true;
                for (const auto &[Category, Label]: categoryOptions) {
                    if (!firstCategory) ImGui::SameLine();
                    firstCategory = false;
                    if (CategoryChip(Label, work.SelectedCategory == Category)) {
                        work.SelectedCategory = Category;
                        work.SelectedImage = -1;
                    }
                }

                ImGui::Spacing();
                ImGui::Text("Available System Images");
                if (isLoading) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("Fetching available images from SDK manager...");
                }
                ImGui::Spacing();

                {
                    PickerTableStyle tableStyle;

                    ImGui::BeginChild("##RemoteImageTableFrame", ImVec2(-1.0f, 280.0f), true, ImGuiWindowFlags_NoScrollbar);
                    if (ImGui::BeginTable("##RemoteImageTable", 5, PickerTableFlags, ImVec2(-1.0f, -1.0f))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn(" Name", ImGuiTableColumnFlags_WidthStretch, 2.7f);
                        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.1f);
                        ImGui::TableSetupColumn("API", ImGuiTableColumnFlags_WidthFixed, 56.0f);
                        ImGui::TableSetupColumn("ABI", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                        ImGui::TableHeadersRow();

                        int visibleCount = 0;
                        if (!isLoading && work.RemoteImages.empty()) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextColored(
                                HexColor(Colors::Negative),
                                "No remote system images found. Check your SDK and internet connection."
                            );
                        } else {
                            for (int i = 0; i < static_cast<int>(work.RemoteImages.size()); i++) {
                                const auto &img = work.RemoteImages[i];
                                if (!MatchesImageFilters(img, work.SearchFilter, work.SelectedCategory)) continue;

                                visibleCount++;
                                const bool isSelected = work.SelectedImage == i;
                                const auto [_, Label, Color] = SystemImageTypeStyleFor(img);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                const std::string label = StrConcat(
                                    " ",
                                    SystemImageDisplayName(img.ApiLevel, img.DisplayName),
                                    "##RemoteImage",
                                    std::to_string(i)
                                );
                                if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 0.0f))) {
                                    work.SelectedImage = i;
                                }
                                if (isSelected) ImGui::SetItemDefaultFocus();

                                ImGui::TableNextColumn();
                                ImGui::TextColored(HexColor(Color), "%s", Label);

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", img.ApiLevel.c_str());

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", img.Abi.c_str());

                                ImGui::TableNextColumn();
                                if (img.IsInstalled) {
                                    ImGui::TextColored(HexColor(Colors::Positive), "Installed");
                                } else {
                                    ImGui::TextDisabled("Available");
                                }
                            }
                        }

                        if (!isLoading && !work.RemoteImages.empty() && visibleCount == 0) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled(" No system images available!");
                        }

                        ImGui::EndTable();
                    }
                    ImGui::EndChild();
                }

                if (isInstalling || removalBusy) ImGui::EndDisabled();

                if (isInstalling && work.Progress) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    float fraction;
                    std::string statusText;
                    {
                        std::lock_guard lock(work.Progress->Mutex);
                        fraction = work.Progress->Percent;
                        statusText = work.Progress->StatusText;
                    }

                    ImGui::Text("%s", statusText.c_str());
                    ImGui::Spacing();

                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, HexColor(Colors::Positive));
                    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));
                    ImGui::PopStyleColor();
                }

                if (!isInstalling && work.Progress) {
                    bool finished;
                    bool succeeded;
                    std::string statusText;
                    {
                        std::lock_guard lock(work.Progress->Mutex);
                        finished = work.Progress->Finished;
                        succeeded = work.Progress->Succeeded;
                        statusText = work.Progress->StatusText;
                    }

                    if (finished) {
                        ImGui::Spacing();
                        const float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
                        ImGui::SetCursorPosX(
                            (ImGui::GetContentRegionAvail().x - textWidth) * 0.5f + ImGui::GetCursorStartPos().x
                        );
                        if (succeeded) ImGui::TextColored(HexColor(Colors::Positive), "%s", statusText.c_str());
                        else ImGui::TextColored(HexColor(Colors::Negative), "%s", statusText.c_str());
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const bool hasVisibleSelection = work.SelectedImage >= 0 &&
                                                 work.SelectedImage < static_cast<int>(work.RemoteImages.size()) &&
                                                 MatchesImageFilters(work.RemoteImages[work.SelectedImage], work.SearchFilter, work.SelectedCategory);

                const bool selectedInstalled = hasVisibleSelection && work.RemoteImages[work.SelectedImage].IsInstalled;
                const bool canUseSelected = !isLoading && !isInstalling && !removalBusy && selectedInstalled;
                const bool canRemove = canUseSelected;
                const bool canInstall = !isLoading && !isInstalling && !removalBusy && hasVisibleSelection && !selectedInstalled;

                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float actionWidth = ImGui::GetContentRegionAvail().x;
                const float halfWidth = (actionWidth - spacing) * 0.5f;
                const float thirdWidth = (actionWidth - spacing * 2.0f) / 3.0f;

                const bool licenseBusy = work.LicenseBusy.load();

                if (!work.LicenseError.empty()) {
                    ImGui::TextColored(HexColor(Colors::Negative), "%s", work.LicenseError.c_str());
                    ImGui::Spacing();
                }

                if (selectedInstalled || removalBusy) {
                    if (removalBusy) {
                        ImGui::BeginDisabled();
                        PositiveButton("Use Selected Image", false, ImVec2(thirdWidth, 0));
                        ImGui::EndDisabled();
                    } else if (PositiveButton("Use Selected Image", canUseSelected, ImVec2(thirdWidth, 0))) {
                        const auto &img = work.RemoteImages[work.SelectedImage];
                        if (SelectInstalledSystemImage(context, img.PackagePath)) {
                            work.Progress.reset();
                            context.UI.ShowInstallImageDialog = false;
                        }
                    }

                    ImGui::SameLine();
                    if (removalBusy) {
                        ImGui::BeginDisabled();
                        NegativeButton("Removing...", false, ImVec2(thirdWidth, 0));
                        ImGui::EndDisabled();
                    } else if (NegativeButton("Remove Image", canRemove, ImVec2(thirdWidth, 0))) {
                        const std::string pkg = work.RemoteImages[work.SelectedImage].PackagePath;
                        removal.Busy = true;
                        removal.Future = std::async(std::launch::async, [&context, pkg] {
                            try {
                                return UninstallSystemImage(context.Host.Sdk, pkg);
                            } catch (...) {
                                return false;
                            }
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton("Close", !isInstalling && !removalBusy, ImVec2(thirdWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (isInstalling) {
                    ImGui::BeginDisabled();
                    PositiveButton("Installing...", false, ImVec2(halfWidth, 0));
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (PrimaryButton("Close", false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else if (licenseBusy) {
                    ImGui::BeginDisabled();
                    PositiveButton("Checking licenses...", false, ImVec2(halfWidth, 0));
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (PrimaryButton("Close", false, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                } else {
                    if (PositiveButton("Install", canInstall, ImVec2(halfWidth, 0))) {
                        const auto &img = work.RemoteImages[work.SelectedImage];
                        work.PendingPackagePath = img.PackagePath;
                        work.LicenseError.clear();
                        work.LicenseBusy = true;
                        work.LicenseCheckFuture = std::async(std::launch::async, [&context] {
                            return CheckSdkLicenses(context.Host.Sdk);
                        });
                    }

                    ImGui::SameLine();
                    if (PrimaryButton("Close", !isInstalling && !removalBusy, ImVec2(halfWidth, 0))) {
                        work.Progress.reset();
                        context.UI.ShowInstallImageDialog = false;
                    }
                }

                ImGui::EndPopup();
            }
        }
    }
}

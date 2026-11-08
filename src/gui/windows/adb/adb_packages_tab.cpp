//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_packages_tab.h"

#include "adb_packages_list.h"
#include "adb_session.h"

#include "../../widgets/log_viewer.h"
#include "app/file_dialog.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::AdbWindow {
    namespace {
        std::string PackageEmptyText(const AdbDeviceSession &session) {
            return OutputPlaceholder(
                session.Busy.load() && IsPackageTask(session.Task),
                session.HasPackageOutput,
                session.PackageOutput,
                BusyText(session.Task),
                Platform::Tr("No package output yet.")
            );
        }

        float PairWidth() {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            return std::max(1.0F, (avail - spacing) * 0.5F);
        }

        void InstallBody(Context &context, const bool usable) {
            auto &session = SelectedSession(context.AdbWork);
            SectionHeading(Platform::Tr("Install APK"));
            const CheckItem options[] = {
                {.Id = "##ReplaceExisting", .Value = &session.InstallReplace, .Label = Platform::Tr("Replace existing")},
                {.Id = "##AllowDowngrade", .Value = &session.InstallDowngrade, .Label = Platform::Tr("Allow downgrade")},
                {.Id = "##GrantPermissions", .Value = &session.InstallGrantPermissions, .Label = Platform::Tr("Grant runtime permissions")},
            };
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float installW = ActionWidth(Platform::Tr("Install"));
            const float pathW = std::max(1.0F, ImGui::GetContentRegionAvail().x - installW - spacing);
            if (BrowseRow("##AdbApkPath", session.ApkPath, Platform::Tr("Choose an APK file..."), "AdbApkBrowse", Platform::Tr("Browse..."), pathW, Platform::Tr("APK"))) {
                const std::string start = DirectoryOf(session.ApkPath);
                const auto picked = PickFromDialog(context, [&] {
                    return App::FileDialog::PickFile(
                        Platform::Tr("Open APK"),
                        Platform::Tr("Android packages"),
                        {"*.apk", "*.APK"},
                        start
                    );
                });

                if (picked) {
                    session.ApkPath = *picked;
                    SetTabStatus(
                        session,
                        AdbWorkspaceTab::PackagesFiles,
                        StatusBarTone::Success,
                        Platform::TrFormat("Selected {0}.", FileNameOf(session.ApkPath))
                    );
                }
            }
            ImGui::SameLine(0.0F, spacing);
            const bool enabled = usable && !session.Busy.load() && !session.ApkPath.empty();
            if (PositiveButton(Platform::Tr("Install"), enabled, ImVec2(installW, 0.0F))) {
                const std::string path = session.ApkPath;
                const Core::AdbInstallOptions installOptions{
                    .Replace = session.InstallReplace,
                    .AllowDowngrade = session.InstallDowngrade,
                    .GrantRuntimePermissions = session.InstallGrantPermissions,
                };
                StartTask(
                    context,
                    session,
                    AdbTask::Install,
                    [path, installOptions](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                        return Core::InstallAdbPackage(sdk, serial, path, installOptions, cancel);
                    }
                );
            }
            Checkboxes(options, IM_ARRAYSIZE(options));
        }

        void PushBody(Context &context, const bool usable, const char *browseLabel, const float buttonWidth) {
            auto &session = SelectedSession(context.AdbWork);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            SectionHeading(Platform::Tr("Push File"));
            const float rowW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float fieldW = std::max(1.0F, rowW - buttonWidth - spacing);
            if (BrowseRow("##PushLocal", session.PushLocalPath, Platform::Tr("Choose a local file..."), "PushBrowse", browseLabel, rowW)) {
                const auto picked = PickFromDialog(context, [] {
                    return App::FileDialog::PickFile(
                        Platform::Tr("Choose File"),
                        Platform::Tr("All files"),
                        {}
                    );
                });

                if (picked) {
                    CopyToBuffer(session.PushLocalPath, sizeof(session.PushLocalPath), *picked);
                }
            }
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint(
                "##PushRemote",
                Platform::Tr("Remote path, for example /sdcard/Download/file"),
                session.PushRemotePath,
                sizeof(session.PushRemotePath)
            );
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(
                    Platform::Tr("Push"),
                    usable && !session.Busy.load() && session.PushLocalPath[0] != '\0' && session.PushRemotePath[0] != '\0',
                    ImVec2(buttonWidth, 0.0F)
                )) {
                const std::string local = session.PushLocalPath;
                const std::string remote = session.PushRemotePath;
                StartTask(context, session, AdbTask::Push, [local, remote](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                    return Core::PushAdbFile(sdk, serial, local, remote, cancel);
                });
            }
        }

        void PullBody(Context &context, const bool usable, const char *browseLabel, const float buttonWidth) {
            auto &session = SelectedSession(context.AdbWork);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            SectionHeading(Platform::Tr("Pull File"));
            const float rowW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float fieldW = std::max(1.0F, rowW - buttonWidth - spacing);
            if (BrowseRow("##PullLocal", session.PullLocalPath, Platform::Tr("Choose a local folder..."), "PullBrowse", browseLabel, rowW)) {
                const std::string start = session.PullLocalPath;
                const auto picked = PickFromDialog(context, [&] {
                    return App::FileDialog::PickDirectory(
                        Platform::Tr("Choose Destination"),
                        start
                    );
                });

                if (picked) {
                    CopyToBuffer(session.PullLocalPath, sizeof(session.PullLocalPath), *picked);
                    SetTabStatus(
                        session,
                        AdbWorkspaceTab::PackagesFiles,
                        StatusBarTone::Success,
                        Platform::TrFormat("Selected {0}.", FileNameOf(*picked))
                    );
                }
            }
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint("##PullRemote", Platform::Tr("Remote file path"), session.PullRemotePath, sizeof(session.PullRemotePath));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(
                    Platform::Tr("Pull"),
                    usable && !session.Busy.load() && session.PullRemotePath[0] != '\0' && session.PullLocalPath[0] != '\0',
                    ImVec2(buttonWidth, 0.0F)
                )) {
                const std::string remote = session.PullRemotePath;
                const std::string local = session.PullLocalPath;
                StartTask(context, session, AdbTask::Pull, [remote, local](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                    return Core::PullAdbFile(sdk, serial, remote, local, cancel);
                });
            }
        }

        void UninstallBody(Context &context, const bool usable) {
            auto &session = SelectedSession(context.AdbWork);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            SectionHeading(Platform::Tr("Uninstall Package"));
            const float browseW = ActionWidth(Platform::Tr("Browse..."));
            const float uninstallW = ActionWidth(Platform::Tr("Uninstall"));
            const float fieldW = std::max(1.0F, ImGui::GetContentRegionAvail().x - browseW - uninstallW - (spacing * 2.0F));
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint("##PackageName", Platform::Tr("Package name"), session.PackageName, sizeof(session.PackageName));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(Platform::Tr("Browse..."), usable && !session.Busy.load(), ImVec2(browseW, 0.0F))) {
                OpenPackageList(context, session);
            }
            ImGui::SameLine(0.0F, spacing);
            if (NegativeButton(Platform::Tr("Uninstall"), usable && !session.Busy.load() && session.PackageName[0] != '\0', ImVec2(uninstallW, 0.0F))) {
                session.ShowUninstallDialog = true;
            }
            SubtitledCheckbox("##KeepAppData", &session.UninstallKeepData, Platform::Tr("Keep app data"));
        }
    }

    void PackageFileTab(Context &context, const Core::AdbDevice *device) {
        auto &session = SelectedSession(context.AdbWork);
        const bool usable = DeviceUsable(device);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float column = PairWidth();
        FittedCardPair(
            "##InstallApk",
            [&] { InstallBody(context, usable); },
            "##UninstallPackage",
            [&] { UninstallBody(context, usable); },
            column,
            spacing
        );
        ImGui::Spacing();

        const char *browseLabel = Platform::Tr("Browse...");
        const float transferButtonW = std::max(
            Em(12.0F),
            ImGui::CalcTextSize(browseLabel).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
        );
        const float transferColumn = PairWidth();
        FittedCardPair(
            "##PushFile",
            [&] { PushBody(context, usable, browseLabel, transferButtonW); },
            "##PullFile",
            [&] { PullBody(context, usable, browseLabel, transferButtonW); },
            transferColumn,
            spacing
        );

        ImGui::Spacing();

        const std::string empty = PackageEmptyText(session);
        LogViewer(
            "AdbPackageOutput",
            session.PackageOutput,
            session.HasPackageOutput,
            session.PackageView,
            {
                .EmptyText = empty.c_str(),
                .AutoScroll = false,
                .MinBodyHeight = Eh(6.0F),
            }
        );
    }

    void ConfirmUninstall(Context &context) {
        auto &session = SelectedSession(context.AdbWork);
        if (!session.ShowUninstallDialog) {
            return;
        }
        const DialogResult result = SimpleDialog({
            .Id = "ConfirmAdbUninstall",
            .IsOpen = session.ShowUninstallDialog,
            .Title = Platform::Tr("Uninstall Package?"),
            .Message = Platform::Tr("The selected package will be removed from this device."),
            .ConfirmButtonTitle = Platform::Tr("Uninstall"),
            .CancelButtonTitle = Platform::Tr("Cancel"),
            .Type = DialogType::Negative,
            .CloseOnConfirm = true,
        });
        if (result == DialogResult::Confirmed) {
            session.ShowUninstallDialog = false;
            const std::string package = session.PackageName;
            const bool keepData = session.UninstallKeepData;
            StartTask(context, session, AdbTask::Uninstall, [package, keepData](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                return Core::UninstallAdbPackage(sdk, serial, package, keepData, cancel);
            });
        }
    }
}

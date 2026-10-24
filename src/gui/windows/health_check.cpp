//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#include <chrono>
#include <future>
#include <vector>

#include "imgui.h"

#include "accept_licenses.h"
#include "health_check.h"
#include "install_image.h"
#include "onboarding.h"
#include "../widgets.h"
#include "../theme.h"
#include "platform/i18n.h"
#include "app/app_settings.h"

namespace CoreDeck::Gui {
    namespace {
        struct StatusStyle {
            const char *Icon;
            const char *Color;
        };

        StatusStyle StatusStyleFor(const Core::HealthStatus status) {
            switch (status) {
                case Core::HealthStatus::Passed:
                    return {.Icon = Icons::CHECK_CIRCLE, .Color = Colors::POSITIVE};
                case Core::HealthStatus::Warning:
                    return {.Icon = Icons::WARNING_TRIANGLE, .Color = Colors::WARNING};
                case Core::HealthStatus::Failed:
                    return {.Icon = Icons::TIMES_CIRCLE, .Color = Colors::NEGATIVE};
                case Core::HealthStatus::Running:
                    return {.Icon = Icons::REFRESH, .Color = Colors::ACCENT_INFO};
                case Core::HealthStatus::Pending:
                case Core::HealthStatus::Skipped:
                default:
                    return {.Icon = Icons::CIRCLE, .Color = Colors::TEXT_MUTED};
            }
        }

        const char *FixButtonLabel(const Core::HealthFix fix) {
            switch (fix) {
                case Core::HealthFix::InstallSdk:
                    return Platform::Tr("Install SDK...");
                case Core::HealthFix::InstallCmdlineTools:
                    return Platform::Tr("Install Tools...");
                case Core::HealthFix::InstallBuildTools:
                    return Platform::Tr("Install Build Tools...");
                case Core::HealthFix::ConfigureJdk:
                    return Platform::Tr("Configure JDK...");
                case Core::HealthFix::AcceptLicenses:
                    return Platform::Tr("Accept Licenses...");
                case Core::HealthFix::InstallSystemImage:
                    return Platform::Tr("Install Image...");
                case Core::HealthFix::None:
                default:
                    return "";
            }
        }

        void CloseDialog(Context &context) {
            context.UI.ShowHealthCheckDialog = false;
            ImGui::CloseCurrentPopup();
        }

        void ApplyFix(Context &context, const Core::HealthFix fix) {
            switch (fix) {
                case Core::HealthFix::InstallSdk:
                    CloseDialog(context);
                    OpenSdkSetupWizard(context);
                    break;
                case Core::HealthFix::InstallCmdlineTools:
                    CloseDialog(context);
                    OpenCmdlineToolsInstall(context);
                    break;
                case Core::HealthFix::InstallBuildTools:
                    CloseDialog(context);
                    OpenBuildToolsInstall(context);
                    break;
                case Core::HealthFix::ConfigureJdk:
                    CloseDialog(context);
                    context.UI.ShowPreferences = true;
                    context.UI.OpenPreferencesToJava = true;
                    break;
                case Core::HealthFix::AcceptLicenses:
                    OpenAcceptLicensesDialog(context);
                    break;
                case Core::HealthFix::InstallSystemImage:
                    CloseDialog(context);
                    context.UI.ReopenCreateAvdOnInstallClose = false;
                    OpenInstallImageDialog(context);
                    break;
                case Core::HealthFix::None:
                default:
                    break;
            }
        }

        void PublishLicenseNotice(Context &context, const std::vector<Core::HealthCheckResult> &items) {
            for (const auto &item: items) {
                if (item.Id != Core::HealthCheckId::Licenses) {
                    continue;
                }

                auto &notice = context.LicenseNotice;
                if (item.Status == Core::HealthStatus::Failed && item.Fix == Core::HealthFix::AcceptLicenses) {
                    notice.Epoch += 1;
                    notice.Status = Core::LicenseStatus::SomeUnaccepted;
                    notice.Known = true;
                    notice.SdkManagerPath = context.Host.Sdk.SdkManagerPath;
                } else if (item.Status == Core::HealthStatus::Passed) {
                    notice.Epoch += 1;
                    notice.Status = Core::LicenseStatus::AllAccepted;
                    notice.Known = true;
                    notice.SdkManagerPath = context.Host.Sdk.SdkManagerPath;
                }
                return;
            }
        }

        void PollWork(Context &context) {
            auto &work = context.HealthCheckWork;

            if (work.Busy.load() && work.Future.valid() &&
                work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                work.Future.get();
                work.Busy = false;

                std::vector<Core::HealthCheckResult> items;
                if (work.Progress) {
                    const std::scoped_lock lock(work.Progress->Mutex);
                    items = work.Progress->Items;
                }
                PublishLicenseNotice(context, items);
            }
        }

        ImVec4 DescriptionColorFor(const Core::HealthStatus status) {
            switch (status) {
                case Core::HealthStatus::Failed:
                    return HexColor(Colors::NEGATIVE);
                case Core::HealthStatus::Warning:
                    return HexColor(Colors::WARNING);
                case Core::HealthStatus::Running:
                    return HexColor(Colors::ACCENT_INFO);
                case Core::HealthStatus::Passed:
                    return HexColor(Colors::TEXT_SUBTLE);
                case Core::HealthStatus::Pending:
                case Core::HealthStatus::Skipped:
                default:
                    return HexColor(Colors::TEXT_MUTED);
            }
        }

        void HealthChecks(Context &context, const std::vector<Core::HealthCheckResult> &items, const bool actionsEnabled) {
            for (const auto &item: items) {
                const auto [icon, color] = StatusStyleFor(item.Status);
                const bool showAction =
                    item.Fix != Core::HealthFix::None &&
                    (item.Status == Core::HealthStatus::Failed || item.Status == Core::HealthStatus::Warning);
                const char *description = item.Detail.empty() ? Core::HealthStatusLabel(item.Status) : item.Detail.c_str();

                ImGui::PushID(static_cast<int>(item.Id));
                if (StatusActionItem(
                        "##check",
                        icon,
                        HexColor(color),
                        Core::HealthCheckLabel(item.Id),
                        description,
                        DescriptionColorFor(item.Status),
                        showAction ? FixButtonLabel(item.Fix) : nullptr,
                        actionsEnabled
                    )) {
                    ApplyFix(context, item.Fix);
                }
                ImGui::PopID();
            }
        }

        std::string ObservedSdkTools(const Core::SdkInfo &sdk) {
            return sdk.AvdManagerPath + "\n" + sdk.SdkManagerPath + "\n" + sdk.ApkAnalyzerPath + "\n" + sdk.Aapt2Path +
                   "\n" + sdk.ZipalignPath + "\n" + sdk.ApksignerPath;
        }
    }

    void OpenHealthCheckDialog(Context &context) {
        auto &work = context.HealthCheckWork;
        context.UI.ShowHealthCheckDialog = true;
        if (context.Host.Sdk.SdkPath.empty()) {
            const auto toolEnv = context.Host.Sdk.ToolEnv;
            context.Host.Sdk = Core::DetectAndroidSdk(App::LoadAppSettings().SdkPath);
            context.Host.Sdk.ToolEnv = toolEnv;
        } else {
            Core::RefreshAndroidSdk(context.Host.Sdk);
        }
        const Core::SdkInfo &sdk = context.Host.Sdk;
        work.ObservedTools = ObservedSdkTools(sdk);
        if (work.Busy.load()) {
            return;
        }

        work.Progress = std::make_shared<Core::HealthCheckProgressData>();
        work.Busy = true;
        work.Future = std::async(
            std::launch::async,
            [sdk, jdk = context.Host.Jdk, progress = work.Progress] {
                Core::RunHealthChecks(sdk, jdk, progress);
            }
        );
    }

    void BuildHealthCheckWindow(Context &context) {
        PollWork(context);

        if (!context.UI.ShowHealthCheckDialog) {
            return;
        }

        const Core::SdkInfo &sdk = context.Host.Sdk;
        const std::string toolsNow = ObservedSdkTools(sdk);
        if (!context.HealthCheckWork.Busy.load() && context.HealthCheckWork.ObservedTools != toolsNow) {
            OpenHealthCheckDialog(context);
        }

        const std::string title = Platform::TrWindow("Health Check", "HealthCheckDialog");
        const bool licenseOpen = context.UI.ShowAcceptLicensesDialog || context.AcceptLicensesWork.Busy.load();
        bool *healthOpen = licenseOpen ? nullptr : &context.UI.ShowHealthCheckDialog;
        if (BeginCenteredModal(
                title.c_str(),
                healthOpen,
                ImVec2(Em(92.0F), 0.0F),
                WINDOW_AUTO_RESIZE_FLAGS | ImGuiWindowFlags_NoScrollbar
            )) {
            const auto &work = context.HealthCheckWork;
            const bool busy = work.Busy.load();

            std::vector<Core::HealthCheckResult> items;
            if (work.Progress) {
                const std::scoped_lock lock(work.Progress->Mutex);
                items = work.Progress->Items;
            }

            HealthChecks(context, items, !busy && !licenseOpen);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            if (PositiveButton(busy ? Platform::Tr("Checking...") : Platform::Tr("Run Again"), !busy && !licenseOpen, ImVec2(halfWidth, 0), busy)) {
                OpenHealthCheckDialog(context);
            }
            ImGui::SameLine();
            if (PrimaryButton(Platform::Tr("Close"), !licenseOpen, ImVec2(halfWidth, 0))) {
                CloseDialog(context);
            }

            BuildAcceptLicensesWindow(context);
            ImGui::EndPopup();
        }
    }
}

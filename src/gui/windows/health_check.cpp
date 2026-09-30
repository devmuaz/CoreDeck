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
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        struct StatusStyle {
            const char *Icon;
            const char *Color;
        };

        StatusStyle StatusStyleFor(const HealthStatus status) {
            switch (status) {
                case HealthStatus::Passed:
                    return {.Icon = Icons::CHECK_CIRCLE, .Color = Colors::POSITIVE};
                case HealthStatus::Warning:
                    return {.Icon = Icons::WARNING_TRIANGLE, .Color = Colors::WARNING};
                case HealthStatus::Failed:
                    return {.Icon = Icons::TIMES_CIRCLE, .Color = Colors::NEGATIVE};
                case HealthStatus::Running:
                    return {.Icon = Icons::REFRESH, .Color = Colors::ACCENT_INFO};
                case HealthStatus::Pending:
                case HealthStatus::Skipped:
                default:
                    return {.Icon = Icons::CIRCLE, .Color = Colors::TEXT_MUTED};
            }
        }

        const char *FixButtonLabel(const HealthFix fix) {
            switch (fix) {
                case HealthFix::InstallSdk:
                    return Tr("Install SDK...");
                case HealthFix::InstallCmdlineTools:
                    return Tr("Install Tools...");
                case HealthFix::InstallBuildTools:
                    return Tr("Install Build Tools...");
                case HealthFix::ConfigureJdk:
                    return Tr("Configure JDK...");
                case HealthFix::AcceptLicenses:
                    return Tr("Accept Licenses...");
                case HealthFix::InstallSystemImage:
                    return Tr("Install Image...");
                case HealthFix::None:
                default:
                    return "";
            }
        }

        void CloseDialog(Context &context) {
            context.UI.ShowHealthCheckDialog = false;
            ImGui::CloseCurrentPopup();
        }

        void ApplyFix(Context &context, const HealthFix fix) {
            switch (fix) {
                case HealthFix::InstallSdk:
                    CloseDialog(context);
                    OpenSdkSetupWizard(context);
                    break;
                case HealthFix::InstallCmdlineTools:
                    CloseDialog(context);
                    OpenCmdlineToolsInstall(context);
                    break;
                case HealthFix::InstallBuildTools:
                    CloseDialog(context);
                    OpenBuildToolsInstall(context);
                    break;
                case HealthFix::ConfigureJdk:
                    CloseDialog(context);
                    context.UI.ShowPreferences = true;
                    context.UI.OpenPreferencesToJava = true;
                    break;
                case HealthFix::AcceptLicenses:
                    OpenAcceptLicensesDialog(context);
                    break;
                case HealthFix::InstallSystemImage:
                    CloseDialog(context);
                    context.UI.ReopenCreateAvdOnInstallClose = false;
                    OpenInstallImageDialog(context);
                    break;
                case HealthFix::None:
                default:
                    break;
            }
        }

        void PublishLicenseNotice(Context &context, const std::vector<HealthCheckResult> &items) {
            for (const auto &item: items) {
                if (item.Id != HealthCheckId::Licenses) {
                    continue;
                }

                auto &notice = context.LicenseNotice;
                if (item.Status == HealthStatus::Failed && item.Fix == HealthFix::AcceptLicenses) {
                    notice.Epoch += 1;
                    notice.Status = LicenseStatus::SomeUnaccepted;
                    notice.Known = true;
                    notice.SdkManagerPath = context.Host.Sdk.SdkManagerPath;
                } else if (item.Status == HealthStatus::Passed) {
                    notice.Epoch += 1;
                    notice.Status = LicenseStatus::AllAccepted;
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

                std::vector<HealthCheckResult> items;
                if (work.Progress) {
                    std::scoped_lock lock(work.Progress->Mutex);
                    items = work.Progress->Items;
                }
                PublishLicenseNotice(context, items);
            }
        }

        ImVec4 DescriptionColorFor(const HealthStatus status) {
            switch (status) {
                case HealthStatus::Failed:
                    return HexColor(Colors::NEGATIVE);
                case HealthStatus::Warning:
                    return HexColor(Colors::WARNING);
                case HealthStatus::Running:
                    return HexColor(Colors::ACCENT_INFO);
                case HealthStatus::Passed:
                    return HexColor(Colors::TEXT_SUBTLE);
                case HealthStatus::Pending:
                case HealthStatus::Skipped:
                default:
                    return HexColor(Colors::TEXT_MUTED);
            }
        }

        void DrawChecks(Context &context, const std::vector<HealthCheckResult> &items, const bool actionsEnabled) {
            for (const auto &item: items) {
                const auto [icon, color] = StatusStyleFor(item.Status);
                const bool showAction =
                    item.Fix != HealthFix::None &&
                    (item.Status == HealthStatus::Failed || item.Status == HealthStatus::Warning);
                const char *description = item.Detail.empty() ? HealthStatusLabel(item.Status) : item.Detail.c_str();

                ImGui::PushID(static_cast<int>(item.Id));
                if (StatusActionItem(
                        "##check",
                        icon,
                        HexColor(color),
                        HealthCheckLabel(item.Id),
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
    }

    void OpenHealthCheckDialog(Context &context) {
        auto &work = context.HealthCheckWork;
        context.UI.ShowHealthCheckDialog = true;
        RefreshAndroidSdk(context.Host.Sdk);
        const SdkInfo &sdk = context.Host.Sdk;
        work.ObservedTools = sdk.AvdManagerPath + "\n" + sdk.SdkManagerPath + "\n" + sdk.ApkAnalyzerPath;
        if (work.Busy.load()) {
            return;
        }

        work.Progress = std::make_shared<HealthCheckProgressData>();
        work.Busy = true;
        work.Future = std::async(
            std::launch::async,
            [sdk, jdk = context.Host.Jdk, progress = work.Progress] {
                RunHealthChecks(sdk, jdk, progress);
            }
        );
    }

    void BuildHealthCheckWindow(Context &context) {
        PollWork(context);

        if (!context.UI.ShowHealthCheckDialog) {
            return;
        }

        const SdkInfo &sdk = context.Host.Sdk;
        const std::string toolsNow = sdk.AvdManagerPath + "\n" + sdk.SdkManagerPath + "\n" + sdk.ApkAnalyzerPath;
        if (!context.HealthCheckWork.Busy.load() && context.HealthCheckWork.ObservedTools != toolsNow) {
            OpenHealthCheckDialog(context);
        }

        const std::string title = TrWindow("Health Check", "HealthCheckDialog");
        const bool licenseOpen = context.UI.ShowAcceptLicensesDialog || context.AcceptLicensesWork.Busy.load();
        bool *healthOpen = licenseOpen ? nullptr : &context.UI.ShowHealthCheckDialog;
        if (BeginCenteredModal(
                title.c_str(),
                healthOpen,
                ImVec2(Em(92.0F), 0.0F),
                WINDOW_AUTO_RESIZE_FLAGS | ImGuiWindowFlags_NoScrollbar
            )) {
            auto &work = context.HealthCheckWork;
            const bool busy = work.Busy.load();

            std::vector<HealthCheckResult> items;
            bool finished = false;
            if (work.Progress) {
                std::scoped_lock lock(work.Progress->Mutex);
                items = work.Progress->Items;
                finished = work.Progress->Finished;
            }

            DrawChecks(context, items, !busy && !licenseOpen);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            if (PositiveButton(busy ? Tr("Checking...") : Tr("Run Again"), !busy && !licenseOpen, ImVec2(halfWidth, 0), busy)) {
                OpenHealthCheckDialog(context);
            }
            ImGui::SameLine();
            if (PrimaryButton(Tr("Close"), !licenseOpen, ImVec2(halfWidth, 0))) {
                CloseDialog(context);
            }

            BuildAcceptLicensesWindow(context);
            ImGui::EndPopup();
        }
    }
}

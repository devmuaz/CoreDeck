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
                    return "Install SDK...";
                case HealthFix::InstallCmdlineTools:
                    return "Install Tools...";
                case HealthFix::ConfigureJdk:
                    return "Configure JDK...";
                case HealthFix::AcceptLicenses:
                    return "Accept Licenses...";
                case HealthFix::InstallSystemImage:
                    return "Install Image...";
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
                    std::lock_guard lock(work.Progress->Mutex);
                    items = work.Progress->Items;
                }
                PublishLicenseNotice(context, items);
            }
        }

        void DrawSummary(const bool busy, const bool finished, const std::vector<HealthCheckResult> &items) {
            if (busy || !finished) {
                ImGui::TextColored(HexColor(Colors::TEXT_SUBTLE), "Checking your setup...");
                return;
            }

            switch (OverallHealth(items)) {
                case HealthStatus::Passed:
                    ImGui::TextColored(
                        HexColor(Colors::POSITIVE),
                        "Everything looks good. Your setup is ready."
                    );
                    break;
                case HealthStatus::Warning:
                    ImGui::TextColored(
                        HexColor(Colors::WARNING),
                        "Your setup works, but some checks reported warnings."
                    );
                    break;
                case HealthStatus::Failed:
                default:
                    ImGui::TextColored(
                        HexColor(Colors::NEGATIVE),
                        "Some checks failed. Use the Fix buttons to resolve them."
                    );
                    break;
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
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
            ImGui::BeginChild("##HealthChecks", ImVec2(0.0F, Eh(22.0F)), 0);

            for (const auto &item: items) {
                const auto [Icon, Color] = StatusStyleFor(item.Status);
                const bool showAction =
                    item.Fix != HealthFix::None &&
                    (item.Status == HealthStatus::Failed || item.Status == HealthStatus::Warning);
                const char *description = item.Detail.empty() ? HealthStatusLabel(item.Status) : item.Detail.c_str();

                ImGui::PushID(static_cast<int>(item.Id));
                if (StatusActionItem(
                        "##check",
                        Icon,
                        HexColor(Color),
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

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }

    void OpenHealthCheckDialog(Context &context) {
        auto &work = context.HealthCheckWork;
        context.UI.ShowHealthCheckDialog = true;
        if (work.Busy.load()) {
            return;
        }

        work.Progress = std::make_shared<HealthCheckProgressData>();
        work.Busy = true;
        work.Future = std::async(
            std::launch::async,
            [sdk = context.Host.Sdk, jdk = context.Host.Jdk, progress = work.Progress] {
                RunHealthChecks(sdk, jdk, progress);
            }
        );
    }

    void BuildHealthCheckWindow(Context &context) {
        PollWork(context);

        if (!context.UI.ShowHealthCheckDialog) {
            return;
        }

        constexpr auto TITLE = "Health Check###HealthCheckDialog";
        if (!ImGui::IsPopupOpen(TITLE)) {
            ImGui::OpenPopup(TITLE);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(EmV(92.0F, 24.0F), ImGuiCond_Appearing);

        const bool licenseOpen = context.UI.ShowAcceptLicensesDialog || context.AcceptLicensesWork.Busy.load();
        bool *healthOpen = licenseOpen ? nullptr : &context.UI.ShowHealthCheckDialog;
        if (RoundedBeginPopupModal(TITLE, healthOpen, WINDOW_AUTO_RESIZE_FLAGS)) {
            auto &work = context.HealthCheckWork;
            const bool busy = work.Busy.load();

            std::vector<HealthCheckResult> items;
            bool finished = false;
            if (work.Progress) {
                std::lock_guard lock(work.Progress->Mutex);
                items = work.Progress->Items;
                finished = work.Progress->Finished;
            }

            DrawSummary(busy, finished, items);
            ImGui::Spacing();

            DrawChecks(context, items, !busy && !licenseOpen);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5F;

            if (PositiveButton(busy ? "Checking..." : "Run Again", !busy && !licenseOpen, ImVec2(halfWidth, 0))) {
                OpenHealthCheckDialog(context);
            }
            ImGui::SameLine();
            if (PrimaryButton("Close", !licenseOpen, ImVec2(halfWidth, 0))) {
                CloseDialog(context);
            }

            BuildAcceptLicensesWindow(context);
            ImGui::EndPopup();
        }
    }
}

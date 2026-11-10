//
// Created by AbdulMuaz Aqeel on 23/09/2026.
//

#include <chrono>
#include <future>

#include "imgui.h"

#include "gui/sdk/accept_licenses_dialog.h"
#include "core/sdk/sdk.h"
#include "gui/sdk/health_check_dialog.h"
#include "gui/theme/theme.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"
#include "platform/desktop.h"

namespace CoreDeck::Gui {
    namespace {
        void MarkLicensesAccepted(Context &context) {
            auto &notice = context.Dialogs.AcceptLicenses.Notice;
            notice.Epoch += 1;
            notice.Status = Core::LicenseStatus::AllAccepted;
            notice.Known = true;
            notice.SdkManagerPath = context.Host.Sdk.SdkManagerPath;
        }

        void RefreshHealthCheck(Context &context) {
            if (context.Dialogs.HealthCheck.Open) {
                OpenHealthCheckDialog(context);
            }
        }

        void LicenseConsent(Context &context) {
            auto &work = context.Dialogs.AcceptLicenses.State;
            const bool busy = work.Busy.load();

            ImGui::TextWrapped(
                "%s",
                Platform::Tr(
                    "Some Android SDK package licenses have not been accepted yet. "
                    "By clicking Agree, you confirm that you have read and accept "
                    "Google's current Android SDK license terms."
                )
            );
            ImGui::Spacing();
            if (ImGui::TextLink(Platform::Tr("View License Terms..."))) {
                Platform::OpenUrl(Core::SDK_LICENSE_URL);
            }
            if (busy) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Platform::Tr("Recording acceptance with the SDK Manager..."));
            }

            if (!work.Error.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", work.Error.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            if (PositiveButton(busy ? Platform::Tr("Accepting...") : Platform::Tr("Agree"), true, ImVec2(halfWidth, 0), busy)) {
                work.Error.clear();
                work.Busy = true;
                const Core::SdkInfo sdk = context.Host.Sdk;
                work.Future = std::async(std::launch::async, [sdk] {
                    return Core::AcceptSdkLicenses(sdk);
                });
            }
            ImGui::SameLine();
            if (NegativeButton(Platform::Tr("Cancel"), !busy, ImVec2(halfWidth, 0))) {
                context.Dialogs.AcceptLicenses.Open = false;
                work.Error.clear();
                ImGui::CloseCurrentPopup();
                RefreshHealthCheck(context);
            }
        }
    }

    void OpenAcceptLicensesDialog(Context &context) {
        context.Dialogs.AcceptLicenses.Open = true;
        context.Dialogs.AcceptLicenses.State.Accepted = false;
        context.Dialogs.AcceptLicenses.State.Error.clear();
    }

    void BuildAcceptLicensesDialog(Context &context) {
        auto &work = context.Dialogs.AcceptLicenses.State;

        if (work.Busy.load() && work.Future.valid() &&
            work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const bool ok = work.Future.get();
            work.Busy = false;
            if (ok) {
                work.Accepted = true;
            } else {
                work.Error = Platform::Tr("License acceptance failed. Try again or accept via Android Studio.");
            }
        }

        if (!context.Dialogs.AcceptLicenses.Open && !work.Accepted) {
            return;
        }

        const std::string title = Platform::TrWindow("Accept SDK Licenses", "AcceptLicensesDialog");
        const bool busy = work.Busy.load();
        bool *pOpen = busy ? nullptr : &context.Dialogs.AcceptLicenses.Open;
        if (BeginCenteredModal(title.c_str(), pOpen, EmV(56.0F, 14.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            if (work.Accepted) {
                work.Accepted = false;
                context.Dialogs.AcceptLicenses.Open = false;
                work.Error.clear();
                MarkLicensesAccepted(context);
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                RefreshHealthCheck(context);
                return;
            }
            if (!context.Dialogs.AcceptLicenses.Open) {
                ImGui::EndPopup();
                RefreshHealthCheck(context);
                return;
            }

            LicenseConsent(context);
            ImGui::EndPopup();
        }
    }
}

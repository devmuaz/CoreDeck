//
// Created by AbdulMuaz Aqeel on 23/09/2026.
//

#include <chrono>
#include <future>

#include "imgui.h"

#include "accept_licenses.h"
#include "health_check.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        void MarkLicensesAccepted(Context &context) {
            auto &notice = context.LicenseNotice;
            notice.Epoch += 1;
            notice.Status = LicenseStatus::AllAccepted;
            notice.Known = true;
            notice.SdkManagerPath = context.Host.Sdk.SdkManagerPath;
        }

        void RefreshHealthCheck(Context &context) {
            if (context.UI.ShowHealthCheckDialog) {
                OpenHealthCheckDialog(context);
            }
        }

        void DrawConsent(Context &context) {
            auto &work = context.AcceptLicensesWork;
            const bool busy = work.Busy.load();

            LicenseConsentNotice(
                Tr(
                    "Some Android SDK package licenses have not been accepted yet. "
                    "By clicking Agree, you confirm that you have read and accept "
                    "Google's current Android SDK license terms."
                ),
                busy
            );

            if (!work.Error.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", work.Error.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);

            if (PositiveButton(Tr("Agree"), !busy, ImVec2(halfWidth, 0))) {
                work.Error.clear();
                work.Busy = true;
                const SdkInfo sdk = context.Host.Sdk;
                work.Future = std::async(std::launch::async, [sdk] {
                    return AcceptSdkLicenses(sdk);
                });
            }
            ImGui::SameLine();
            if (NegativeButton(Tr("Cancel"), !busy, ImVec2(halfWidth, 0))) {
                context.UI.ShowAcceptLicensesDialog = false;
                work.Error.clear();
                ImGui::CloseCurrentPopup();
                RefreshHealthCheck(context);
            }
        }
    }

    void OpenAcceptLicensesDialog(Context &context) {
        context.UI.ShowAcceptLicensesDialog = true;
        context.AcceptLicensesWork.Accepted = false;
        context.AcceptLicensesWork.Error.clear();
    }

    void BuildAcceptLicensesWindow(Context &context) {
        auto &work = context.AcceptLicensesWork;

        if (work.Busy.load() && work.Future.valid() &&
            work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const bool ok = work.Future.get();
            work.Busy = false;
            if (ok) {
                work.Accepted = true;
            } else {
                work.Error = Tr("License acceptance failed. Try again or accept via Android Studio.");
            }
        }

        if (!context.UI.ShowAcceptLicensesDialog && !work.Accepted) {
            return;
        }

        const std::string title = TrWindow("Accept SDK Licenses", "AcceptLicensesDialog");
        const bool busy = work.Busy.load();
        bool *pOpen = busy ? nullptr : &context.UI.ShowAcceptLicensesDialog;
        if (BeginCenteredModal(title.c_str(), pOpen, EmV(56.0F, 14.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            if (work.Accepted) {
                work.Accepted = false;
                context.UI.ShowAcceptLicensesDialog = false;
                work.Error.clear();
                MarkLicensesAccepted(context);
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                RefreshHealthCheck(context);
                return;
            }
            if (!context.UI.ShowAcceptLicensesDialog) {
                ImGui::EndPopup();
                RefreshHealthCheck(context);
                return;
            }

            DrawConsent(context);
            ImGui::EndPopup();
        }
    }
}

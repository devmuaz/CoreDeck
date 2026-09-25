//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <chrono>
#include <future>
#include <string>

#include "health_check_banner.h"
#include "health_check.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/jdk.h"
#include "../../core/sdk_manager.h"

namespace CoreDeck {
    namespace {
        void PollLicenseNotice(Context &context) {
            auto &notice = context.LicenseNotice;
            const SdkInfo &sdk = context.Host.Sdk;
            const bool canCheck = sdk.IsFound && !sdk.SdkManagerPath.empty();

            if (notice.Busy.load() && notice.Future.valid() &&
                notice.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const LicenseStatus status = notice.Future.get();
                const bool current = notice.PendingEpoch == notice.Epoch;
                notice.Busy = false;
                if (current) {
                    notice.Status = status;
                    notice.Known = true;
                }
            }

            if (!canCheck) {
                if (!notice.Busy.load()) {
                    notice.Known = false;
                    notice.SdkManagerPath.clear();
                }
                return;
            }

            if (notice.SdkManagerPath != sdk.SdkManagerPath) {
                notice.Known = false;
                notice.SdkManagerPath = sdk.SdkManagerPath;
                notice.Epoch += 1;
            }

            if (!notice.Known && !notice.Busy.load()) {
                notice.Busy = true;
                notice.PendingEpoch = notice.Epoch;
                notice.Future = std::async(std::launch::async, [sdk] {
                    return CheckSdkLicenses(sdk);
                });
            }
        }
    }

    void BuildHealthCheckBanner(Context &context) {
        PollLicenseNotice(context);

        const SdkInfo &sdk = context.Host.Sdk;
        const JdkInfo &jdk = context.Host.Jdk;
        const auto &notice = context.LicenseNotice;

        const bool sdkMissing = !sdk.IsFound;
        const bool managersMissing = sdk.IsFound && (sdk.AvdManagerPath.empty() || sdk.SdkManagerPath.empty());
        const bool analyzerMissing = sdk.IsFound && sdk.ApkAnalyzerPath.empty();
        const bool toolsMissing = managersMissing || analyzerMissing;
        const bool jdkIncompatible = sdk.IsFound && jdk.IsFound && !jdk.IsValid;
        const bool licensesUnaccepted =
            notice.Known && notice.Status == LicenseStatus::SomeUnaccepted && !toolsMissing;

        if (!sdkMissing && !toolsMissing && !jdkIncompatible && !licensesUnaccepted) {
            context.UI.HideHealthCheckBanner = false;
            return;
        }
        if (context.UI.HideHealthCheckBanner) {
            return;
        }

        const char *title = "Android SDK not found";
        std::string detail =
            "No working Android SDK was found. The emulator binary is missing, or the path is invalid.";
        if (!sdkMissing && toolsMissing && jdkIncompatible) {
            title = "SDK tools and Java need attention";
            detail = "The command-line tools are missing, and the detected Java is too old to run them.";
        } else if (!sdkMissing && managersMissing) {
            title = "Command-line tools are missing";
            detail = "avdmanager and sdkmanager are missing, so the AVD list, device profiles, and system images cannot be loaded.";
        } else if (!sdkMissing && analyzerMissing) {
            title = "APK Analyzer is missing";
            detail = "apkanalyzer is missing from the command-line tools, so APK Analyzer cannot read packages.";
        } else if (!sdkMissing && jdkIncompatible) {
            title = "Java is too old for the SDK tools";
            detail =
                "The detected Java (" +
                (jdk.VersionString.empty() ? std::string("unknown version") : jdk.VersionString) +
                ") is too old for avdmanager and sdkmanager, which need JDK " +
                std::to_string(JDK_MINIMUM_MAJOR) +
                " or newer.";
        } else if (licensesUnaccepted) {
            title = "SDK licenses are not accepted";
            detail =
                "Some Android SDK package licenses have not been accepted. "
                "Run a health check to review them and accept the terms.";
        }

        const std::string healthLabel = IconWithLabel(Icons::HEART_PULSE, "Run Health Check");
        const Banner banner{
            .Id = "##HealthCheckBanner",
            .Tone = BannerTone::Warning,
            .Icon = Icons::WARNING_TRIANGLE,
            .Title = title,
            .Subtitle = detail.c_str(),
            .ActionLabel = healthLabel.c_str(),
        };
        switch (ShowBanner(banner)) {
            case BannerResult::Action:
                OpenHealthCheckDialog(context);
                break;
            case BannerResult::Dismissed:
                context.UI.HideHealthCheckBanner = true;
                break;
            case BannerResult::None:
                break;
        }
    }
}

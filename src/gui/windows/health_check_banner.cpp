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
#include "../../core/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void PollLicenseNotice(Context &context) {
            auto &notice = context.LicenseNotice;
            const Core::SdkInfo &sdk = context.Host.Sdk;
            const bool canCheck = sdk.IsFound && !sdk.SdkManagerPath.empty();

            if (notice.Busy.load() && notice.Future.valid() &&
                notice.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const Core::LicenseStatus status = notice.Future.get();
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
                    return Core::CheckSdkLicenses(sdk);
                });
            }
        }
    }

    void BuildHealthCheckBanner(Context &context) {
        PollLicenseNotice(context);

        const Core::SdkInfo &sdk = context.Host.Sdk;
        const Core::JdkInfo &jdk = context.Host.Jdk;
        const auto &notice = context.LicenseNotice;

        const bool sdkMissing = !sdk.IsFound;
        const bool managersMissing = sdk.IsFound && (sdk.AvdManagerPath.empty() || sdk.SdkManagerPath.empty());
        const bool analyzerMissing = sdk.IsFound && sdk.ApkAnalyzerPath.empty();
        const bool toolsMissing = managersMissing || analyzerMissing;
        const bool jdkIncompatible = sdk.IsFound && jdk.IsFound && !jdk.IsValid;
        const bool buildToolsMissing = sdk.IsFound && !toolsMissing && !jdkIncompatible &&
                                       (sdk.Aapt2Path.empty() || sdk.ZipalignPath.empty() || sdk.ApksignerPath.empty());
        const bool licensesUnaccepted =
            notice.Known && notice.Status == Core::LicenseStatus::SomeUnaccepted && !toolsMissing && !buildToolsMissing;

        if (!sdkMissing && !toolsMissing && !jdkIncompatible && !buildToolsMissing && !licensesUnaccepted) {
            context.UI.HideHealthCheckBanner = false;
            return;
        }
        if (context.UI.HideHealthCheckBanner) {
            return;
        }

        const char *title = Core::Tr("Android SDK not found");
        std::string detail =
            Core::Tr("No working Android SDK was found. The emulator binary is missing, or the path is invalid.");
        if (!sdkMissing && toolsMissing && jdkIncompatible) {
            title = Core::Tr("SDK tools and Java need attention");
            detail = Core::Tr("The command-line tools are missing, and the detected Java is too old to run them.");
        } else if (!sdkMissing && managersMissing) {
            title = Core::Tr("Command-line tools are missing");
            detail = Core::Tr("avdmanager and sdkmanager are missing, so the AVD list, device profiles, and system images cannot be loaded.");
        } else if (!sdkMissing && analyzerMissing) {
            title = Core::Tr("APK Analyzer is missing");
            detail = Core::Tr("apkanalyzer is missing from the command-line tools, so APK Analyzer cannot read packages.");
        } else if (buildToolsMissing) {
            title = Core::Tr("Build tools are missing");
            detail = Core::Tr("aapt2, zipalign, or apksigner were not found under build-tools, so APK Analyzer and APK Signer cannot use them.");
        } else if (!sdkMissing && jdkIncompatible) {
            title = Core::Tr("Java is too old for the SDK tools");
            detail = Core::TrFormat(
                "The detected Java ({0}) is too old for avdmanager and sdkmanager, which need JDK {1} or newer.",
                jdk.VersionString.empty() ? Core::Tr("unknown version") : jdk.VersionString.c_str(),
                std::to_string(Core::JDK_MINIMUM_MAJOR)
            );
        } else if (licensesUnaccepted) {
            title = Core::Tr("SDK licenses are not accepted");
            detail = Core::Tr(
                "Some Android SDK package licenses have not been accepted. "
                "Run a health check to review them and accept the terms."
            );
        }

        const std::string healthLabel = IconWithLabel(Icons::HEART_PULSE, Core::Tr("Run Health Check"));
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

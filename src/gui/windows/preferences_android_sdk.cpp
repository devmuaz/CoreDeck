//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include <future>
#include <optional>
#include <string>

#include "imgui.h"

#include "preferences_android_sdk.h"
#include "preferences.h"
#include "../application.h"
#include "../utilities.h"
#include "../widgets.h"
#include "../../core/jdk.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        enum class SdkPathAction : uint8_t {
            Apply,
            Discover,
        };

        struct SdkPathResult {
            Core::SdkInfo Sdk;
            AvdCatalogSnapshot Catalog;
            bool UpdateBuffer = false;
        };

        struct SdkPathJob {
            std::future<SdkPathResult> Future;
            SdkPathAction Action = SdkPathAction::Apply;
        };

        SdkPathJob &PathJob() {
            static SdkPathJob job;
            return job;
        }

        std::optional<std::string> &PendingSdkPath() {
            static std::optional<std::string> path;
            return path;
        }

        void StartSdkPathJob(const SdkPathAction action, const Core::JdkInfo &jdk) {
            SupersedeAvdListRefresh();
            SdkPathJob &job = PathJob();
            job.Action = action;
            const bool updateBuffer = action == SdkPathAction::Discover;
            job.Future = std::async(std::launch::async, [jdk, updateBuffer] {
                SdkPathResult result;
                result.Sdk = Core::DetectAndroidSdk();
                Core::ApplyJdkToSdk(result.Sdk, jdk);
                result.Catalog = LoadAvdCatalog(result.Sdk);
                result.UpdateBuffer = updateBuffer;
                return result;
            });
        }
    }

    void PollPreferencesSdkWork(Context &context) {
        SdkPathJob &job = PathJob();
        if (!job.Future.valid()) {
            return;
        }
        if (job.Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }

        SdkPathResult result = job.Future.get();
        SupersedeAvdListRefresh();
        context.Host.Sdk = std::move(result.Sdk);
        context.Host.Manager.SetSdk(context.Host.Sdk);
        ApplyAvdCatalog(context, std::move(result.Catalog));
        context.UI.HideHealthCheckBanner = false;
        if (result.UpdateBuffer) {
            PendingSdkPath() = context.Host.Sdk.SdkPath;
        }
        PersistAppSettings(context);
    }

    void PreferencesAndroidSdkSection(const Context &context, char *sdkPathBuffer, const size_t bufferSize) {
        if (const std::optional<std::string> &pending = PendingSdkPath()) {
            CopyToBuffer(sdkPathBuffer, bufferSize, *pending);
            PendingSdkPath().reset();
        }

        PreferencesSectionHeader(Core::Tr("Android SDK"), Core::Tr("Where CoreDeck looks for the emulator and command-line tools."));

        const std::string pathStr = PathPicker(
            "##SdkPrefs",
            Core::Tr("SDK root"),
            Core::Tr("Path to Android SDK"),
            Core::Tr("Select Android SDK directory"),
            sdkPathBuffer,
            bufferSize
        );
        const bool pathOk = Paths::Onboarding::ValidateSdkPath(pathStr);

        if (!pathStr.empty()) {
            if (pathOk) {
                StatusMessage(StatusMessageTone::Positive, Core::Tr("Valid Android SDK path."));
            } else {
                StatusMessage(
                    StatusMessageTone::Error,
                    Core::Tr("Not a valid SDK (need emulator and cmdline-tools with avdmanager).")
                );
            }
        } else {
            StatusMessage(
                StatusMessageTone::Info,
                Core::Tr("Leave empty to auto-detect from ANDROID_HOME or default install paths.")
            );
        }

        ImGui::Spacing();
        ImGui::Spacing();

        const SdkPathJob &pathJob = PathJob();
        const bool pathBusy = pathJob.Future.valid();
        const bool applyingPath = pathBusy && pathJob.Action == SdkPathAction::Apply;
        const bool discoveringPath = pathBusy && pathJob.Action == SdkPathAction::Discover;

        if (PrimaryButton(Core::Tr("Apply SDK Path"), (!pathBusy && pathOk) || applyingPath, ImVec2(0, 0), applyingPath)) {
            Paths::Onboarding::SaveSdkPathOverride(pathStr);
            StartSdkPathJob(SdkPathAction::Apply, context.Host.Jdk);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !pathOk) {
            ImGui::SetTooltip("%s", Core::Tr("Fix the path or validation errors before applying."));
        }

        ImGui::SameLine();
        if (PrimaryButton(Core::Tr("Use Default Discovery"), !pathBusy || discoveringPath, ImVec2(0, 0), discoveringPath)) {
            Paths::Onboarding::ClearSdkPathOverride();
            StartSdkPathJob(SdkPathAction::Discover, context.Host.Jdk);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Core::Tr("Forget the saved override and detect the SDK from ANDROID_HOME / default paths."));
        }
    }
}

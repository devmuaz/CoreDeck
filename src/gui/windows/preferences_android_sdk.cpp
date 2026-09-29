//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include <cstring>

#include "imgui.h"

#include "preferences_android_sdk.h"
#include "preferences.h"
#include "../application.h"
#include "../widgets.h"
#include "../../core/jdk.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    void DrawPreferencesAndroidSdkSection(Context &context, char *sdkPathBuffer, const size_t bufferSize) {
        PreferencesSectionHeader(Tr("Android SDK"), Tr("Where CoreDeck looks for the emulator and command-line tools."));

        const std::string pathStr = PathPicker(
            "##SdkPrefs",
            Tr("SDK root"),
            Tr("Path to Android SDK"),
            Tr("Select Android SDK directory"),
            sdkPathBuffer,
            bufferSize
        );
        const bool pathOk = Paths::Onboarding::ValidateSdkPath(pathStr);

        if (!pathStr.empty()) {
            if (pathOk) {
                StatusMessage(StatusMessageTone::Positive, Tr("Valid Android SDK path."));
            } else {
                StatusMessage(
                    StatusMessageTone::Error,
                    Tr("Not a valid SDK (need emulator and cmdline-tools with avdmanager).")
                );
            }
        } else {
            StatusMessage(
                StatusMessageTone::Info,
                Tr("Leave empty to auto-detect from ANDROID_HOME or default install paths.")
            );
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (PrimaryButton(Tr("Apply SDK Path"), pathOk)) {
            Paths::Onboarding::SaveSdkPathOverride(pathStr);
            context.Host.Sdk = DetectAndroidSdk();
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            RefreshAvds(context);
            context.UI.HideHealthCheckBanner = false;
            PersistAppSettings(context);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !pathOk) {
            ImGui::SetTooltip("%s", Tr("Fix the path or validation errors before applying."));
        }

        ImGui::SameLine();
        if (PrimaryButton(Tr("Use Default Discovery"), true)) {
            Paths::Onboarding::ClearSdkPathOverride();
            context.Host.Sdk = DetectAndroidSdk();
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            RefreshAvds(context);
            context.UI.HideHealthCheckBanner = false;
            const std::string &p = context.Host.Sdk.SdkPath;
            strncpy(sdkPathBuffer, p.c_str(), bufferSize - 1);
            sdkPathBuffer[bufferSize - 1] = '\0';
            PersistAppSettings(context);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Tr("Forget the saved override and detect the SDK from ANDROID_HOME / default paths."));
        }
    }
}

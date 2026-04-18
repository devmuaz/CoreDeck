//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "preferences.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/file_dialog.h"

namespace CoreDeck {
    void BuildPreferencesWindow(Context &context) {
        if (context.UI.ShowPreferences && !ImGui::IsPopupOpen("Preferences###CoreDeckPrefs")) {
            ImGui::OpenPopup("Preferences###CoreDeckPrefs");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking;

        static char sdkPathBuffer[2048];

        if (ImGui::BeginPopupModal("Preferences###CoreDeckPrefs", &context.UI.ShowPreferences, flags)) {
            if (ImGui::IsWindowAppearing()) {
                const std::string &p = context.Host.Sdk.SdkPath;
                strncpy(sdkPathBuffer, p.c_str(), sizeof(sdkPathBuffer) - 1);
                sdkPathBuffer[sizeof(sdkPathBuffer) - 1] = '\0';
            }

            ImGui::TextUnformatted("General");
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Checkbox("Auto-scroll output log", &context.Logs.AutoScroll)) {
                PersistAppSettings(context);
            }
            if (ImGui::Checkbox("Confirm before deleting an AVD", &context.Prefs.ConfirmBeforeDeleteAvd)) {
                PersistAppSettings(context);
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Android SDK");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextUnformatted("SDK root");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
            ImGui::InputTextWithHint("##sdk_pref", "Path to Android SDK", sdkPathBuffer, sizeof(sdkPathBuffer));
            ImGui::SameLine();
            if (PrimaryButton("Browse...", true, ImVec2(110, 0))) {
                if (const auto picked = FileDialog::PickFolder("Select Android SDK folder", sdkPathBuffer)) {
                    strncpy(sdkPathBuffer, picked->c_str(), sizeof(sdkPathBuffer) - 1);
                    sdkPathBuffer[sizeof(sdkPathBuffer) - 1] = '\0';
                }
            }

            const std::string pathStr = sdkPathBuffer;
            const bool pathOk = Paths::Onboarding::ValidateSdkPath(pathStr);
            if (!pathStr.empty()) {
                if (pathOk) {
                    ImGui::TextColored(HexColor("#33CC47"), "This folder looks like a valid Android SDK.");
                } else {
                    ImGui::TextColored(
                        HexColor("#E64D40"),
                        "Not a valid SDK (need emulator and cmdline-tools with avdmanager)."
                    );
                }
            }

            ImGui::Spacing();

            constexpr float applyW = 160.0f;
            constexpr float defaultW = 180.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float totalW = applyW + spacing + defaultW;
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - totalW + ImGui::GetCursorPosX());

            if (PrimaryButton("Apply SDK path", pathOk, ImVec2(applyW, 0))) {
                Paths::Onboarding::SaveSdkPathOverride(pathStr);
                context.Host.Sdk = DetectAndroidSdk();
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                context.UI.HideInvalidSdkPathBanner = false;
                PersistAppSettings(context);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !pathOk) {
                ImGui::SetTooltip("Fix the path or validation errors before applying.");
            }

            ImGui::SameLine();
            if (PrimaryButton("Use default discovery", true, ImVec2(defaultW, 0))) {
                Paths::Onboarding::ClearSdkPathOverride();
                context.Host.Sdk = DetectAndroidSdk();
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                context.UI.HideInvalidSdkPathBanner = false;
                const std::string &p = context.Host.Sdk.SdkPath;
                strncpy(sdkPathBuffer, p.c_str(), sizeof(sdkPathBuffer) - 1);
                sdkPathBuffer[sizeof(sdkPathBuffer) - 1] = '\0';
                PersistAppSettings(context);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Forget the saved override and detect the SDK from ANDROID_HOME / default paths.");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing(); {
                constexpr float closeW = 120.0f;
                ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - closeW + ImGui::GetCursorPosX());
                if (PrimaryButton("Close", true, ImVec2(closeW, 0))) {
                    context.UI.ShowPreferences = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }
}

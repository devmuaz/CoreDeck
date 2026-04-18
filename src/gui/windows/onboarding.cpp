//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "onboarding.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/file_dialog.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"

namespace CoreDeck {
    enum class Step {
        Welcome,
        SdkSetup
    };

    static void CenteredText(const char *text, const ImVec4 &color) {
        const float width = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5f);
        ImGui::TextColored(color, "%s", text);
    }

    static void VerticalCenter(const float contentHeight) {
        const float available = ImGui::GetContentRegionAvail().y;
        const float offset = (available - contentHeight) * 0.5f;
        if (offset > 0.0f) ImGui::Dummy(ImVec2(0, offset));
    }

    static void BuildWelcomeStep(Step &step) {
        VerticalCenter(260.0f);

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        CenteredText("CoreDeck", HexColor("#F2F2F2"));
        ImGui::PopFont();

        ImGui::Spacing();
        CenteredText("Your Android emulator command center.", HexColor("#66666B"));

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        const auto welcomeLine1 = "Welcome! CoreDeck helps you manage Android emulators";
        const auto welcomeLine2 = "faster and cleaner than the default tooling.";
        CenteredText(welcomeLine1, HexColor("#A8A8AD"));
        CenteredText(welcomeLine2, HexColor("#A8A8AD"));

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        constexpr float buttonWidth = 180.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
        if (PositiveButton("Get Started", true, ImVec2(buttonWidth, 0))) {
            step = Step::SdkSetup;
        }
    }

    static void BuildSdkSetupStep(Context &context, Step &step, char *pathBuffer, size_t pathBufferSize) {
        VerticalCenter(320.0f);

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        CenteredText("Locate your Android SDK", HexColor("#F2F2F2"));
        ImGui::PopFont();

        ImGui::Spacing();
        CenteredText("CoreDeck needs to know where your Android SDK lives.", HexColor("#66666B"));
        CenteredText("This is where 'emulator', 'avdmanager' and system images are installed.", HexColor("#66666B"));

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        constexpr float formWidth = 600.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - formWidth) * 0.5f);
        ImGui::BeginGroup();

        ImGui::Text("SDK path");
        ImGui::SetNextItemWidth(formWidth - 110.0f);
        ImGui::InputTextWithHint("##sdk_path", "e.g. /Users/you/Library/Android/sdk", pathBuffer, pathBufferSize);
        ImGui::SameLine();
        if (PrimaryButton("Browse...", true, ImVec2(100, 0))) {
            const auto picked = FileDialog::PickFolder("Select your Android SDK folder", pathBuffer);
            if (picked.has_value()) {
                strncpy(pathBuffer, picked->c_str(), pathBufferSize - 1);
                pathBuffer[pathBufferSize - 1] = '\0';
            }
        }

        ImGui::Spacing();
        const std::string currentPath = pathBuffer;
        const bool isValid = Paths::Onboarding::ValidateSdkPath(currentPath);
        if (!currentPath.empty()) {
            if (isValid) {
                ImGui::TextColored(
                    HexColor("#33CC47"),
                    "%s", "Looks good. Found the Android emulator at this location."
                );
            } else {
                ImGui::TextColored(
                    HexColor("#E64D40"), "%s",
                    "Couldn't find the Android emulator here. Make sure this is your SDK root folder."
                );
            }
        } else {
            ImGui::TextColored(
                HexColor("#66666B"), "%s",
                "Choose the folder containing your Android SDK (cmdline-tools, emulator, platform-tools, etc)."
            );
        }

        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        constexpr float footerWidth = 280.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - footerWidth) * 0.5f);
        ImGui::BeginGroup();

        if (PrimaryButton("Back", true, ImVec2(130, 0))) step = Step::Welcome;

        ImGui::SameLine();
        if (PositiveButton("Continue", isValid, ImVec2(130, 0))) {
            Paths::Onboarding::SaveSdkPathOverride(currentPath);
            Paths::Onboarding::MarkFirstRunComplete();

            context.Host.Sdk = DetectAndroidSdk();
            RefreshAvds(context);
            context.Flow.CurrentScreen = Screen::Main;
        }

        ImGui::EndGroup();
    }

    void BuildOnboardingWindow(Context &context) {
        static auto step = Step::Welcome;
        static char pathBuffer[1024] = {};
        static bool initialized = false;

        if (!initialized) {
            if (!context.Host.Sdk.SdkPath.empty()) {
                strncpy(pathBuffer, context.Host.Sdk.SdkPath.c_str(), sizeof(pathBuffer) - 1);
                pathBuffer[sizeof(pathBuffer) - 1] = '\0';
            }
            initialized = true;
        }

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("##Onboarding", nullptr, flags);

        switch (step) {
            case Step::Welcome:
                BuildWelcomeStep(step);
                break;
            case Step::SdkSetup:
                BuildSdkSetupStep(context, step, pathBuffer, sizeof(pathBuffer));
                break;
        }

        ImGui::End();
    }
}

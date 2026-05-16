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
    namespace {
        enum class Step : uint8_t {
            Welcome,
            SdkSetup
        };

        void CenteredText(const char *text, const ImVec4 &color) {
            const float width = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5F);
            ImGui::TextColored(color, "%s", text);
        }

        void VerticalCenter(const float contentHeight) {
            const float available = ImGui::GetContentRegionAvail().y;
            const float offset = (available - contentHeight) * 0.5F;
            if (offset > 0.0F) {
                ImGui::Dummy(ImVec2(0, offset));
            }
        }

        void BuildWelcomeStep(Step &step) {
            VerticalCenter(260.0F);

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            CenteredText(COREDECK_TITLE, HexColor(Colors::TEXT_PRIMARY));
            ImGui::PopFont();

            ImGui::Spacing();
            CenteredText("Your Android emulator command center.", HexColor(Colors::TEXT_MUTED));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            constexpr const char *WELCOME_LINE1 = "Welcome! CoreDeck helps you manage Android emulators";
            constexpr const char *WELCOME_LINE2 = "faster and cleaner than the default tooling.";
            CenteredText(WELCOME_LINE1, HexColor(Colors::TEXT_SUBTLE));
            CenteredText(WELCOME_LINE2, HexColor(Colors::TEXT_SUBTLE));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float BUTTON_WIDTH = Em(20.0F);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - BUTTON_WIDTH) * 0.5F);
            if (PositiveButton("Get Started", true, ImVec2(BUTTON_WIDTH, 0))) {
                step = Step::SdkSetup;
            }
        }

        void BuildSdkSetupStep(Context &context, Step &step, char *pathBuffer, size_t pathBufferSize) {
            VerticalCenter(320.0F);

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            CenteredText("Locate your Android SDK", HexColor(Colors::TEXT_PRIMARY));
            ImGui::PopFont();

            ImGui::Spacing();
            CenteredText("CoreDeck needs to know where your Android SDK lives.", HexColor(Colors::TEXT_MUTED));
            CenteredText("This is where 'emulator', 'avdmanager' and system images are installed.", HexColor(Colors::TEXT_MUTED));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float FORM_WIDTH = Em(66.0F);
            const float BROWSE_WIDTH = Em(11.0F);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - FORM_WIDTH) * 0.5F);
            ImGui::BeginGroup();

            ImGui::Text("SDK path");
            ImGui::SetNextItemWidth(FORM_WIDTH - BROWSE_WIDTH - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputTextWithHint("##sdk_path", "e.g. /Users/you/Library/Android/sdk", pathBuffer, pathBufferSize);
            ImGui::SameLine();
            if (PrimaryButton("Browse...", true, ImVec2(BROWSE_WIDTH, 0))) {
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
                        HexColor(Colors::POSITIVE),
                        "%s",
                        "Looks good. Found the Android emulator at this location."
                    );
                } else {
                    ImGui::TextColored(
                        HexColor(Colors::NEGATIVE),
                        "%s",
                        "Couldn't find the Android emulator here. Make sure this is your SDK root folder."
                    );
                }
            } else {
                ImGui::TextColored(
                    HexColor(Colors::TEXT_MUTED),
                    "%s",
                    "Choose the folder containing your Android SDK (cmdline-tools, emulator, platform-tools, etc)."
                );
            }

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float FOOTER_BUTTON_WIDTH = Em(14.0F);
            const float FOOTER_WIDTH = (FOOTER_BUTTON_WIDTH * 2.0F) + ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - FOOTER_WIDTH) * 0.5F);
            ImGui::BeginGroup();

            if (PrimaryButton("Back", true, ImVec2(FOOTER_BUTTON_WIDTH, 0))) {
                step = Step::Welcome;
            }

            ImGui::SameLine();
            if (PositiveButton("Continue", isValid, ImVec2(FOOTER_BUTTON_WIDTH, 0))) {
                Paths::Onboarding::SaveSdkPathOverride(currentPath);
                Paths::Onboarding::MarkFirstRunComplete();

                context.Host.Sdk = DetectAndroidSdk();
                RefreshAvds(context);
                context.Flow.CurrentScreen = Screen::Main;
            }

            ImGui::EndGroup();
        }
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

        constexpr ImGuiWindowFlags FLAGS =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("##Onboarding", nullptr, FLAGS);

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

//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <chrono>
#include <cstring>
#include <future>

#include "imgui.h"

#include "onboarding.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/file_dialog.h"
#include "../../core/jdk.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/sdk_bootstrap.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    namespace {
        enum class Step : uint8_t {
            Welcome,
            SdkChoice,
            SdkLocate,
            SdkInstallRoot,
            SdkInstallJdk,
            SdkInstalling,
            SdkInstallFailed,
        };

        struct WizardState {
            Step CurrentStep = Step::Welcome;
            bool ReturnToMainOnCancel = false;
            char SdkPathBuffer[1024] = {};
            char InstallRootBuffer[1024] = {};
            char JdkPathBuffer[1024] = {};
            bool Initialized = false;
        };

        WizardState &Wizard() {
            static WizardState state;
            return state;
        }

        void CopyToBuffer(char *buffer, const size_t size, const std::string &value) {
            strncpy(buffer, value.c_str(), size - 1);
            buffer[size - 1] = '\0';
        }

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

        void StepTitle(const char *title, const char *subtitle = nullptr, const char *subtitle2 = nullptr) {
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            CenteredText(title, HexColor(Colors::TEXT_PRIMARY));
            ImGui::PopFont();

            if (subtitle) {
                ImGui::Spacing();
                CenteredText(subtitle, HexColor(Colors::TEXT_MUTED));
            }
            if (subtitle2) {
                CenteredText(subtitle2, HexColor(Colors::TEXT_MUTED));
            }
        }

        void BeginCenteredGroup(const float width) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5F);
            ImGui::BeginGroup();
        }

        std::string PathPicker(
            const char *id,
            const char *label,
            const char *hint,
            const char *dialogTitle,
            char *buffer,
            const size_t bufferSize,
            const float formWidth
        ) {
            const float browseWidth = Em(11.0F);
            ImGui::Text("%s", label);
            ImGui::SetNextItemWidth(formWidth - browseWidth - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
            ImGui::SameLine();
            if (PrimaryButton(StrConcat("Browse...##", id).c_str(), true, ImVec2(browseWidth, 0))) {
                if (const auto picked = FileDialog::PickFolder(dialogTitle, buffer); picked.has_value()) {
                    CopyToBuffer(buffer, bufferSize, picked.value());
                }
            }
            return buffer;
        }

        void CommitSdkPath(Context &context, const std::string &sdkPath) {
            Paths::Onboarding::SaveSdkPathOverride(sdkPath);
            Paths::Onboarding::MarkFirstRunComplete();

            context.Host.Sdk = DetectAndroidSdk();
            context.Host.Jdk = DetectJdk();
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);

            RefreshAvds(context);
            context.Flow.CurrentScreen = Screen::Main;
            Wizard().CurrentStep = Step::Welcome;
            Wizard().ReturnToMainOnCancel = false;
        }

        void StartBootstrap(Context &context, const std::string &installRoot) {
            auto &work = context.SdkBootstrapWork;

            work.Plan = BootstrapPlan{.InstallRoot = installRoot};
            work.Progress = std::make_shared<BootstrapProgressData>();
            work.LastError = BootstrapError::None;
            work.LastErrorDetail.clear();
            work.Busy = true;

            work.Future = std::async(
                std::launch::async,
                [plan = work.Plan, jdk = context.Host.Jdk, progress = work.Progress] {
                    return BootstrapAndroidSdk(plan, jdk, progress);
                }
            );
        }

        void BuildWelcomeStep() {
            VerticalCenter(260.0F);

            StepTitle(COREDECK_TITLE, "Your Android emulator command center.");

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

            const float buttonWidth = Em(20.0F);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5F);
            if (PositiveButton("Get Started", true, ImVec2(buttonWidth, 0))) {
                Wizard().CurrentStep = Step::SdkChoice;
            }
        }

        void BuildSdkChoiceStep(Context &context) {
            VerticalCenter(300.0F);

            StepTitle(
                "Set up the Android SDK",
                "CoreDeck runs the official emulator, avdmanager and sdkmanager tools.",
                "Point it at an SDK you already have, or let CoreDeck download the tools."
            );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            const bool platformSupported = !GetBundledCmdlineToolsRelease().DownloadUrl.empty();

            if (PositiveButton("Install the Android SDK for me", platformSupported, ImVec2(formWidth, Eh(2.0F)))) {
                Wizard().CurrentStep = Step::SdkInstallRoot;
            }
            ImGui::TextColored(
                HexColor(Colors::TEXT_MUTED),
                "%s",
                platformSupported
                    ? "Downloads Google's official command-line tools, then installs platform-tools and the emulator."
                    : "Google does not publish command-line tools for this platform."
            );

            ImGui::Spacing();
            ImGui::Spacing();

            if (PrimaryButton("I already have an Android SDK", true, ImVec2(formWidth, Eh(2.0F)))) {
                Wizard().CurrentStep = Step::SdkLocate;
            }
            ImGui::TextColored(
                HexColor(Colors::TEXT_MUTED),
                "%s",
                "Choose an existing SDK folder, for example the one installed by Android Studio."
            );

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float backWidth = Em(14.0F);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - backWidth) * 0.5F);
            if (PrimaryButton(Wizard().ReturnToMainOnCancel ? "Cancel" : "Back", true, ImVec2(backWidth, 0))) {
                if (Wizard().ReturnToMainOnCancel) {
                    Wizard().ReturnToMainOnCancel = false;
                    Wizard().CurrentStep = Step::Welcome;
                    context.Flow.CurrentScreen = Screen::Main;
                } else {
                    Wizard().CurrentStep = Step::Welcome;
                }
            }
        }

        void BuildSdkLocateStep(Context &context) {
            VerticalCenter(320.0F);

            StepTitle(
                "Locate your Android SDK",
                "CoreDeck needs to know where your Android SDK lives.",
                "This is where 'emulator', 'avdmanager' and system images are installed."
            );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            const std::string currentPath = PathPicker(
                "##sdk_path",
                "SDK path",
                "e.g. /Users/you/Library/Android/sdk",
                "Select your Android SDK folder",
                Wizard().SdkPathBuffer,
                sizeof(Wizard().SdkPathBuffer),
                formWidth
            );

            ImGui::Spacing();
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

            const float footerButtonWidth = Em(14.0F);
            const float footerWidth = (footerButtonWidth * 2.0F) + ImGui::GetStyle().ItemSpacing.x;
            BeginCenteredGroup(footerWidth);

            if (PrimaryButton("Back", true, ImVec2(footerButtonWidth, 0))) {
                Wizard().CurrentStep = Step::SdkChoice;
            }

            ImGui::SameLine();
            if (PositiveButton("Continue", isValid, ImVec2(footerButtonWidth, 0))) {
                CommitSdkPath(context, currentPath);
            }

            ImGui::EndGroup();
        }

        void BuildSdkInstallRootStep(Context &context) {
            VerticalCenter(470.0F);

            const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();

            StepTitle(
                "Install the Android SDK",
                "CoreDeck will download Google's official command-line tools,",
                "then use them to install the platform tools and the emulator."
            );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            const std::string installRoot = PathPicker(
                "##install_root",
                "Install location",
                "SDK install folder",
                "Select where the Android SDK should be installed",
                Wizard().InstallRootBuffer,
                sizeof(Wizard().InstallRootBuffer),
                formWidth
            );

            ImGui::Spacing();
            ImGui::TextColored(
                HexColor(Colors::TEXT_MUTED),
                "Download size: about %s. Around 2 GB of free space is recommended.",
                FormatFileSize(release.DownloadSize).c_str()
            );
            ImGui::TextColored(
                HexColor(Colors::TEXT_MUTED),
                "%s",
                "CoreDeck remembers this location for itself and never changes your PATH or ANDROID_HOME."
            );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + formWidth);
            ImGui::TextWrapped(
                "Installing these packages requires accepting Google's Android SDK license terms. "
                "By clicking Agree & Install, you confirm that you have read and accept the current terms."
            );
            ImGui::PopTextWrapPos();
            if (PrimaryButton("Open license terms in browser")) {
                OpenUrl("https://developer.android.com/studio/terms");
            }

            const JdkInfo &jdk = context.Host.Jdk;
            ImGui::Spacing();
            if (jdk.IsFound && jdk.IsValid) {
                ImGui::TextColored(
                    HexColor(Colors::POSITIVE),
                    "Using Java: %s",
                    jdk.VersionString.empty() ? jdk.JavaHome.c_str() : jdk.VersionString.c_str()
                );
            } else {
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "A JDK %d or newer is required. You'll be asked for one next.",
                    JDK_MINIMUM_MAJOR
                );
            }

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float footerButtonWidth = Em(18.0F);
            const float footerWidth = (footerButtonWidth * 2.0F) + ImGui::GetStyle().ItemSpacing.x;
            BeginCenteredGroup(footerWidth);

            if (PrimaryButton("Back", true, ImVec2(footerButtonWidth, 0))) {
                Wizard().CurrentStep = Step::SdkChoice;
            }

            ImGui::SameLine();
            const bool canContinue = !installRoot.empty() && !release.DownloadUrl.empty();
            if (PositiveButton("Agree & Install", canContinue, ImVec2(footerButtonWidth, 0))) {
                if (!jdk.IsFound || !jdk.IsValid) {
                    Wizard().CurrentStep = Step::SdkInstallJdk;
                } else {
                    StartBootstrap(context, installRoot);
                    Wizard().CurrentStep = Step::SdkInstalling;
                }
            }

            ImGui::EndGroup();
        }

        void BuildSdkInstallJdkStep(Context &context) {
            VerticalCenter(320.0F);

            StepTitle(
                "Select a Java (JDK) installation",
                "The Android command-line tools run on Java.",
                "Choose a JDK 17 or newer so CoreDeck can run sdkmanager and avdmanager."
            );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            const std::string javaHome = PathPicker(
                "##jdk_home",
                "JAVA_HOME",
                "Path to a JDK 17+ installation",
                "Select a JDK directory",
                Wizard().JdkPathBuffer,
                sizeof(Wizard().JdkPathBuffer),
                formWidth
            );

            ImGui::Spacing();
            JdkInfo candidate;
            if (!javaHome.empty()) {
                candidate = InspectJdk(javaHome);
            }

            if (javaHome.empty()) {
                ImGui::TextColored(
                    HexColor(Colors::TEXT_MUTED),
                    "%s",
                    "Pick the folder that contains bin/java (Android Studio ships one under its 'jbr' folder)."
                );
            } else if (!candidate.IsFound) {
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", "No Java runtime was found in this folder.");
            } else if (!candidate.IsValid) {
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "Found %s, which is older than JDK %d.",
                    candidate.VersionString.c_str(),
                    JDK_MINIMUM_MAJOR
                );
            } else {
                ImGui::TextColored(HexColor(Colors::POSITIVE), "Found %s.", candidate.VersionString.c_str());
            }

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float footerButtonWidth = Em(14.0F);
            const float footerWidth = (footerButtonWidth * 2.0F) + ImGui::GetStyle().ItemSpacing.x;
            BeginCenteredGroup(footerWidth);

            if (PrimaryButton("Back", true, ImVec2(footerButtonWidth, 0))) {
                Wizard().CurrentStep = Step::SdkInstallRoot;
            }

            ImGui::SameLine();
            if (PositiveButton("Use this JDK", candidate.IsValid, ImVec2(footerButtonWidth, 0))) {
                Paths::Onboarding::SaveJdkPathOverride(javaHome);
                context.Host.Jdk = DetectJdk();
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                Wizard().CurrentStep = Step::SdkInstallRoot;
            }

            ImGui::EndGroup();
        }

        void BuildSdkInstallingStep(Context &context) {
            auto &work = context.SdkBootstrapWork;

            if (work.Future.valid() &&
                work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const bool succeeded = work.Future.get();
                work.Busy = false;

                if (work.Progress) {
                    std::lock_guard lock(work.Progress->Mutex);
                    work.LastError = work.Progress->Error;
                    work.LastErrorDetail = work.Progress->ErrorDetail;
                }

                if (succeeded) {
                    CommitSdkPath(context, work.Plan.InstallRoot);
                    return;
                }
                Wizard().CurrentStep = Step::SdkInstallFailed;
                return;
            }

            BootstrapStage stage = BootstrapStage::Preparing;
            float percent = 0.0F;
            std::string status;
            std::string detail;
            if (work.Progress) {
                std::lock_guard lock(work.Progress->Mutex);
                stage = work.Progress->Stage;
                percent = work.Progress->Percent;
                status = work.Progress->StatusText;
                detail = work.Progress->DetailText;
            }

            VerticalCenter(260.0F);

            StepTitle("Installing the Android SDK", BootstrapStageLabel(stage));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            ImGui::ProgressBar(percent, ImVec2(formWidth, 0.0F));

            ImGui::Spacing();
            if (!status.empty()) {
                ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), "%s", status.c_str());
            }
            if (!detail.empty()) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + formWidth);
                ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", detail.c_str());
                ImGui::PopTextWrapPos();
            }

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            bool cancelRequested = false;
            if (work.Progress) {
                std::lock_guard lock(work.Progress->Mutex);
                cancelRequested = work.Progress->CancelRequested;
            }

            const float buttonWidth = Em(14.0F);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5F);
            if (NegativeButton(cancelRequested ? "Cancelling..." : "Cancel", !cancelRequested, ImVec2(buttonWidth, 0))) {
                if (work.Progress) {
                    std::lock_guard lock(work.Progress->Mutex);
                    work.Progress->CancelRequested = true;
                }
            }
        }

        void BuildSdkInstallFailedStep(Context &context) {
            const auto &work = context.SdkBootstrapWork;

            VerticalCenter(280.0F);

            StepTitle("The installation didn't finish");

            ImGui::Spacing();
            ImGui::Spacing();

            const float formWidth = Em(66.0F);
            BeginCenteredGroup(formWidth);

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + formWidth);
            ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", BootstrapErrorMessage(work.LastError));
            if (!work.LastErrorDetail.empty()) {
                ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", work.LastErrorDetail.c_str());
            }
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            const float footerButtonWidth = Em(16.0F);
            const float footerWidth = (footerButtonWidth * 2.0F) + ImGui::GetStyle().ItemSpacing.x;
            BeginCenteredGroup(footerWidth);

            if (PrimaryButton("Locate an SDK instead", true, ImVec2(footerButtonWidth, 0))) {
                Wizard().CurrentStep = Step::SdkLocate;
            }
            ImGui::SameLine();
            if (PositiveButton("Try again", true, ImVec2(footerButtonWidth, 0))) {
                Wizard().CurrentStep = work.LastError == BootstrapError::JdkRequired
                                           ? Step::SdkInstallJdk
                                           : Step::SdkInstallRoot;
            }

            ImGui::EndGroup();
        }

        void EnsureInitialized(const Context &context) {
            if (Wizard().Initialized) {
                return;
            }

            if (!context.Host.Sdk.SdkPath.empty()) {
                CopyToBuffer(Wizard().SdkPathBuffer, sizeof(Wizard().SdkPathBuffer), context.Host.Sdk.SdkPath);
            }
            CopyToBuffer(
                Wizard().InstallRootBuffer,
                sizeof(Wizard().InstallRootBuffer),
                Paths::GetAndroidSdkDefaultPath()
            );
            if (!context.Host.Jdk.JavaHome.empty()) {
                CopyToBuffer(Wizard().JdkPathBuffer, sizeof(Wizard().JdkPathBuffer), context.Host.Jdk.JavaHome);
            }
            Wizard().Initialized = true;
        }
    }

    void OpenSdkSetupWizard(Context &context) {
        EnsureInitialized(context);
        Wizard().CurrentStep = Step::SdkChoice;
        Wizard().ReturnToMainOnCancel = true;
        context.Flow.CurrentScreen = Screen::Onboarding;
    }

    void BuildOnboardingWindow(Context &context) {
        EnsureInitialized(context);

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

        switch (Wizard().CurrentStep) {
            case Step::Welcome:
                BuildWelcomeStep();
                break;
            case Step::SdkChoice:
                BuildSdkChoiceStep(context);
                break;
            case Step::SdkLocate:
                BuildSdkLocateStep(context);
                break;
            case Step::SdkInstallRoot:
                BuildSdkInstallRootStep(context);
                break;
            case Step::SdkInstallJdk:
                BuildSdkInstallJdkStep(context);
                break;
            case Step::SdkInstalling:
                BuildSdkInstallingStep(context);
                break;
            case Step::SdkInstallFailed:
                BuildSdkInstallFailedStep(context);
                break;
        }

        ImGui::End();
    }
}

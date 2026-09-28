//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

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
            SdkJdkSelect,
            SdkInstallRoot,
            SdkInstallTools,
            SdkInstallJdk,
            SdkInstalling,
            SdkInstallFailed,
        };

        struct WizardState {
            Step CurrentStep = Step::Welcome;
            bool ReturnToMainOnCancel = false;
            bool CommandLineToolsOnly = false;
            char SdkPathBuffer[1024] = {};
            char InstallRootBuffer[1024] = {};
            char JdkPathBuffer[1024] = {};
            bool AcceptSdkLicense = false;
            bool Initialized = false;
            bool JdksLoaded = false;
            std::vector<JdkInfo> InstalledJdks;
            std::future<std::vector<JdkInfo>> JdkScan;
        };

        WizardState &Wizard() {
            static WizardState state;
            return state;
        }

        void ReturnToMain(Context &context);

        void CopyToBuffer(char *buffer, const size_t size, const std::string &value) {
            strncpy(buffer, value.c_str(), size - 1);
            buffer[size - 1] = '\0';
        }

        constexpr float WIZARD_COLUMN_PX = 560.0F;
        constexpr float WIZARD_PAD_PX = 12.0F;
        constexpr float WIZARD_ROUND_PX = 8.0F;
        constexpr const char *SDK_LICENSE_URL = "https://developer.android.com/studio/terms";
        constexpr const char *JAVA_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java21";

        ImVec4 WizardHeadingColor() {
            return IsLightColorScheme() ? HexColor(Colors::TEXT_PRIMARY) : HexColor(Colors::WHITE);
        }

        ImVec4 WizardLabelColor() {
            return IsLightColorScheme() ? HexColor(Colors::TEXT_MUTED) : HexColor("#8E8E93");
        }

        float g_WizardColumnWidth = -1.0F;
        float g_WizardInnerWidth = -1.0F;

        float WizardColumnWidth() {
            if (g_WizardColumnWidth > 0.0F) {
                return g_WizardColumnWidth;
            }
            const float maxWidth = WIZARD_COLUMN_PX * GetDpiScale();
            return std::min(maxWidth, std::max(1.0F, ImGui::GetContentRegionAvail().x));
        }

        float WizardContentWidth() {
            if (g_WizardInnerWidth > 0.0F) {
                return g_WizardInnerWidth;
            }
            return std::max(1.0F, ImGui::GetContentRegionAvail().x);
        }

        void PlaceWizardColumn(const float contentHeight) {
            const float offset = (ImGui::GetContentRegionAvail().y - contentHeight) * 0.5F;
            if (offset > 0.0F) {
                ImGui::Dummy(ImVec2(0.0F, offset));
            }
            const float width = WizardColumnWidth();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5F);
        }

        struct WizardPanel {
            ImVec2 Origin;
            float Width = 0.0F;
            float Pad = 0.0F;
            float Rounding = 0.0F;
            bool Framed = false;
        };

        void BeginWizardPanel(const char *id, WizardPanel &panel, const bool framed) {
            const float dpi = GetDpiScale();
            panel.Width = WizardColumnWidth();
            panel.Pad = framed ? WIZARD_PAD_PX * dpi : 0.0F;
            panel.Rounding = WIZARD_ROUND_PX * dpi;
            panel.Framed = framed;
            g_WizardInnerWidth = std::max(1.0F, panel.Width - (panel.Pad * 2.0F));

            ImGui::PushID(id);
            if (framed) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, panel.Rounding);
                ImDrawList *drawList = ImGui::GetWindowDrawList();
                drawList->ChannelsSplit(2);
                drawList->ChannelsSetCurrent(1);
            }

            ImGui::BeginGroup();
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            panel.Origin = window->DC.CursorPos;
            window->DC.CursorPos.x += panel.Pad;
            window->DC.CursorPos.y += panel.Pad;
            window->DC.Indent.x += panel.Pad;
        }

        void EndWizardPanel(WizardPanel &panel) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            window->DC.Indent.x -= panel.Pad;
            window->DC.CursorMaxPos.x = ImMax(window->DC.CursorMaxPos.x, panel.Origin.x + panel.Width);
            window->DC.CursorMaxPos.y += panel.Pad;
            ImGui::EndGroup();

            if (panel.Framed) {
                const ImVec2 cardMin = panel.Origin;
                const ImVec2 cardMax(panel.Origin.x + panel.Width, ImGui::GetItemRectMax().y);
                ImDrawList *drawList = ImGui::GetWindowDrawList();
                drawList->ChannelsSetCurrent(0);
                drawList->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(HexColor(Colors::SURFACE1)), panel.Rounding);
                drawList->AddRect(
                    cardMin,
                    cardMax,
                    ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE)),
                    panel.Rounding,
                    0,
                    1.0F
                );
                drawList->ChannelsMerge();
                ImGui::PopStyleVar();
            }

            g_WizardInnerWidth = -1.0F;
            ImGui::PopID();
        }

        void WizardSeparator(const float width) {
            const float thickness = ImMax(ImGui::GetStyle().SeparatorSize, 1.0F);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(pos.x, pos.y + (thickness * 0.5F)),
                ImVec2(pos.x + width, pos.y + (thickness * 0.5F)),
                ImGui::GetColorU32(ImGuiCol_Separator),
                thickness
            );
            ImGui::Dummy(ImVec2(width, thickness));
        }

        // Auto-resize children take their height from the previous frame, so a step's first
        // appearance is placed with a guess and then snaps. Measure off-screen this frame instead.
        template<typename Draw>
        float MeasureWizardContent(const Draw &draw) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            const ImVec2 cursorPos = window->DC.CursorPos;
            const ImVec2 cursorMax = window->DC.CursorMaxPos;
            const ImVec2 idealMax = window->DC.IdealMaxPos;
            const ImVec2 cursorPrev = window->DC.CursorPosPrevLine;
            const ImVec2 currLine = window->DC.CurrLineSize;
            const float currLineText = window->DC.CurrLineTextBaseOffset;
            const ImVec2 prevLine = window->DC.PrevLineSize;
            const float prevLineText = window->DC.PrevLineTextBaseOffset;
            const bool sameLine = window->DC.IsSameLine;
            const float indentX = window->DC.Indent.x;
            const bool setPos = window->DC.IsSetPos;

            window->DC.CursorPos.y = window->Pos.y - 100000.0F;

            ImGui::PushID("##wizard_measure");
            ImGui::BeginDisabled();
            ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
            draw();
            const float height = ImGui::GetItemRectSize().y;
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
            ImGui::EndDisabled();
            ImGui::PopID();

            window->DC.CursorPos = cursorPos;
            window->DC.CursorMaxPos = cursorMax;
            window->DC.IdealMaxPos = idealMax;
            window->DC.CursorPosPrevLine = cursorPrev;
            window->DC.CurrLineSize = currLine;
            window->DC.CurrLineTextBaseOffset = currLineText;
            window->DC.PrevLineSize = prevLine;
            window->DC.PrevLineTextBaseOffset = prevLineText;
            window->DC.IsSameLine = sameLine;
            window->DC.Indent.x = indentX;
            window->DC.IsSetPos = setPos;
            return height;
        }

        template<typename Draw>
        void PresentWizardStep(const Draw &draw) {
            const float maxWidth = WIZARD_COLUMN_PX * GetDpiScale();
            g_WizardColumnWidth = std::min(maxWidth, std::max(1.0F, ImGui::GetContentRegionAvail().x));
            const float contentHeight = MeasureWizardContent(draw);
            PlaceWizardColumn(contentHeight);
            draw();
            g_WizardColumnWidth = -1.0F;
        }

        void WizardHeading(const char *title, const char *subtitle, const float width) {
            ImGui::SetWindowFontScale(1.15F);
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
            ImGui::TextColored(WizardHeadingColor(), "%s", title);
            ImGui::SetWindowFontScale(1.0F);
            if (subtitle != nullptr && subtitle[0] != '\0') {
                ImGui::Spacing();
                ImGui::TextColored(WizardLabelColor(), "%s", subtitle);
            }
            ImGui::PopTextWrapPos();
        }

        bool SetupOptionCard(
            const char *id,
            const char *title,
            const char *body,
            const float width,
            const bool accent,
            const bool enabled
        ) {
            const float dpi = GetDpiScale();
            const float pad = WIZARD_PAD_PX * dpi;
            const float rounding = WIZARD_ROUND_PX * dpi;
            const float textWidth = std::max(1.0F, width - (pad * 2.0F));
            const ImVec2 titleSize = ImGui::CalcTextSize(title, nullptr, false, textWidth);
            const ImVec2 bodySize = ImGui::CalcTextSize(body, nullptr, false, textWidth);
            const float gap = 4.0F * dpi;
            const float height = pad + titleSize.y + gap + bodySize.y + pad;

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            if (!enabled) {
                ImGui::BeginDisabled();
            }
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
            const bool hovered = enabled && ImGui::IsItemHovered();
            if (!enabled) {
                ImGui::EndDisabled();
            }
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            const ImVec2 cardMax(origin.x + width, origin.y + height);
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImU32 fill = ImGui::GetColorU32(HexColor(hovered ? Colors::SURFACE2 : Colors::SURFACE1));
            ImU32 border = ImGui::GetColorU32(HexColor(Colors::BORDER));
            float borderPx = 1.0F * dpi;
            if (!enabled) {
                border = ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE, 0.7F));
            } else if (accent) {
                border = ImGui::GetColorU32(HexColor(Colors::POSITIVE));
                borderPx = 1.5F * dpi;
            } else if (hovered) {
                border = ImGui::GetColorU32(HexColor(Colors::BORDER_HOVER));
            }
            drawList->AddRectFilled(origin, cardMax, fill, rounding);
            drawList->AddRect(origin, cardMax, border, rounding, 0, borderPx);

            const float textAlpha = enabled ? 1.0F : 0.45F;
            ImVec4 titleColor = WizardHeadingColor();
            ImVec4 bodyColor = WizardLabelColor();
            titleColor.w *= textAlpha;
            bodyColor.w *= textAlpha;
            ImFont *font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            drawList->AddText(
                font,
                fontSize,
                ImVec2(origin.x + pad, origin.y + pad),
                ImGui::GetColorU32(titleColor),
                title,
                nullptr,
                textWidth
            );
            drawList->AddText(
                font,
                fontSize,
                ImVec2(origin.x + pad, origin.y + pad + titleSize.y + gap),
                ImGui::GetColorU32(bodyColor),
                body,
                nullptr,
                textWidth
            );
            return enabled && pressed;
        }

        std::string JavaRuntimeLabel(const JdkInfo &jdk) {
            const std::string lower = LowerCopy(jdk.VersionString);
            const char *vendor = lower.find("openjdk") != std::string::npos ? "OpenJDK" : "Java";
            if (jdk.MajorVersion > 0) {
                return StrConcat(vendor, " ", std::to_string(jdk.MajorVersion));
            }
            if (!jdk.VersionString.empty()) {
                return jdk.VersionString;
            }
            return vendor;
        }

        bool WizardPathField(
            const char *id,
            const char *label,
            const char *hint,
            const char *dialogTitle,
            char *buffer,
            const size_t bufferSize
        ) {
            ImGui::TextColored(WizardLabelColor(), "%s", label);
            ImGui::Spacing();

            const float browseWidth = Em(11.0F);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float totalWidth = WizardContentWidth();
            ImGui::SetNextItemWidth(std::max(1.0F, totalWidth - browseWidth - spacing));
            ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
            ImGui::SameLine();
            if (PrimaryButton(StrConcat("Browse...##", id).c_str(), true, ImVec2(browseWidth, 0))) {
                if (const auto picked = FileDialog::PickDirectory(dialogTitle, buffer)) {
                    strncpy(buffer, picked->c_str(), bufferSize - 1);
                    buffer[bufferSize - 1] = '\0';
                }
            }
            return buffer[0] != '\0';
        }

        JdkInfo InspectTypedJdk(const std::string &javaHome) {
            static std::string cachedPath;
            static JdkInfo cached;
            if (javaHome == cachedPath) {
                return cached;
            }
            cachedPath = javaHome;
            cached = javaHome.empty() ? JdkInfo{} : InspectJdk(javaHome);
            return cached;
        }

        bool SameJdkHome(const std::string &left, const std::string &right) {
            if (left == right) {
                return true;
            }
            std::error_code error;
            const std::filesystem::path canonicalLeft = std::filesystem::weakly_canonical(left, error);
            const std::filesystem::path canonicalRight = std::filesystem::weakly_canonical(right, error);
            return !canonicalLeft.empty() && canonicalLeft == canonicalRight;
        }

        void BeginJdkScan() {
            auto &wizard = Wizard();
            if (wizard.JdksLoaded || wizard.JdkScan.valid()) {
                return;
            }
            wizard.JdkScan = std::async(std::launch::async, [] {
                return ListInstalledJdks();
            });
        }

        bool PollJdkScan() {
            auto &wizard = Wizard();
            if (wizard.JdksLoaded) {
                return true;
            }
            if (!wizard.JdkScan.valid()) {
                return false;
            }
            if (wizard.JdkScan.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return false;
            }
            wizard.InstalledJdks = wizard.JdkScan.get();
            wizard.JdksLoaded = true;
            return true;
        }

        void UseInstalledJdk(Context &context, const JdkInfo &jdk) {
            Paths::Onboarding::SaveJdkPathOverride(jdk.JavaHome);
            context.Host.Jdk = jdk;
            context.Host.Jdk.Source = JdkSource::Override;
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            CopyToBuffer(Wizard().JdkPathBuffer, sizeof(Wizard().JdkPathBuffer), context.Host.Jdk.JavaHome);
        }

        bool DrawJavaChoices(Context &context) {
            if (!PollJdkScan()) {
                BeginJdkScan();
                const float width = WizardContentWidth();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
                ImGui::TextColored(WizardLabelColor(), "Looking for installed JDKs...");
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0.0F, 8.0F * GetDpiScale()));
                return false;
            }

            const float width = WizardContentWidth();
            bool anyValid = false;
            for (const auto &jdk: Wizard().InstalledJdks) {
                anyValid = anyValid || jdk.IsValid;
                const bool selected = jdk.IsValid && SameJdkHome(jdk.JavaHome, context.Host.Jdk.JavaHome);
                const std::string title = JavaRuntimeLabel(jdk);
                const std::string body = jdk.IsValid
                                             ? jdk.JavaHome
                                             : StrConcat("Older than JDK ", std::to_string(JDK_MINIMUM_MAJOR), ". ", jdk.JavaHome);
                const std::string id = StrConcat("##jdk_", jdk.JavaHome);
                if (SetupOptionCard(id.c_str(), title.c_str(), body.c_str(), width, selected, jdk.IsValid)) {
                    UseInstalledJdk(context, jdk);
                }
                ImGui::Dummy(ImVec2(0.0F, 8.0F * GetDpiScale()));
            }

            if (!anyValid) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "%s  No JDK was found. Install JDK %d separately, then come back to this step.",
                    Icons::TIMES,
                    JDK_RECOMMENDED_MAJOR
                );
                ImGui::PopTextWrapPos();
                if (ImGui::TextLink("Download Java...")) {
                    OpenUrl(JAVA_DOWNLOAD_URL);
                }
                ImGui::Dummy(ImVec2(0.0F, 8.0F * GetDpiScale()));
            }

            return context.Host.Jdk.IsFound && context.Host.Jdk.IsValid;
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
            if (Wizard().CommandLineToolsOnly) {
                work.Plan.Packages.clear();
            }
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

        void DrawWelcomeStep() {
            WizardPanel panel;
            BeginWizardPanel("##welcome", panel, true);

            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            ImGui::TextColored(WizardLabelColor(), "%s", COREDECK_TITLE);
            ImGui::Dummy(ImVec2(0.0F, 4.0F * dpi));
            WizardHeading("Welcome!", "Your Android emulator command center.", innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            ImGui::TextColored(
                WizardLabelColor(),
                "%s",
                "CoreDeck helps you manage Android emulators faster and cleaner than the default tooling."
            );
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float startWidth = ImGui::CalcTextSize("Get Started").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(rowStart + innerWidth - startWidth);
            if (PositiveButton("Get Started", true, ImVec2(startWidth, 0))) {
                Wizard().AcceptSdkLicense = false;
                Wizard().CurrentStep = Step::SdkChoice;
            }

            EndWizardPanel(panel);
        }

        void BuildWelcomeStep() {
            PresentWizardStep(DrawWelcomeStep);
        }

        void DrawSdkChoiceStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_choice", panel, false);

            const float width = WizardContentWidth();
            WizardHeading(
                "Set up the Android SDK",
                "CoreDeck runs the official emulator, avdmanager, and sdkmanager tools.",
                width
            );

            ImGui::Dummy(ImVec2(0.0F, 8.0F * GetDpiScale()));

            const bool platformSupported = !GetBundledCmdlineToolsRelease().DownloadUrl.empty();
            if (SetupOptionCard(
                    "##install_sdk",
                    "Install Android SDK Automatically",
                    platformSupported
                        ? "Downloads Google's official command-line tools, platform-tools, and emulator."
                        : "Google does not publish command-line tools for this platform.",
                    width,
                    platformSupported,
                    platformSupported
                )) {
                Wizard().CommandLineToolsOnly = false;
                Wizard().CurrentStep = Step::SdkJdkSelect;
            }

            ImGui::Dummy(ImVec2(0.0F, 8.0F * GetDpiScale()));

            if (SetupOptionCard(
                    "##use_existing_sdk",
                    "Use Existing Android SDK",
                    "Point CoreDeck to an existing SDK directory (e.g., Android Studio installation).",
                    width,
                    false,
                    true
                )) {
                Wizard().CurrentStep = Step::SdkLocate;
            }

            ImGui::Dummy(ImVec2(0.0F, 16.0F * GetDpiScale()));

            {
                StyleVar rounding;
                rounding.Push(ImGuiStyleVar_FrameRounding, WIZARD_ROUND_PX * GetDpiScale());
                if (PrimaryButton("Back", true, ImVec2(Em(10.0F), 0))) {
                    if (Wizard().ReturnToMainOnCancel) {
                        Wizard().ReturnToMainOnCancel = false;
                        Wizard().CurrentStep = Step::Welcome;
                        context.Flow.CurrentScreen = Screen::Main;
                    } else {
                        Wizard().CurrentStep = Step::Welcome;
                    }
                }
            }

            EndWizardPanel(panel);
        }

        void BuildSdkChoiceStep(Context &context) {
            BeginJdkScan();
            PresentWizardStep([&] { DrawSdkChoiceStep(context); });
        }

        void DrawSdkLocateStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_locate", panel, true);

            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            WizardHeading(
                "Locate your Android SDK",
                "Choose the folder that contains emulator, avdmanager, and your system images.",
                innerWidth
            );

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardPathField(
                "##sdk_path",
                "SDK location",
                "e.g. /Users/you/Library/Android/sdk",
                "Select your Android SDK folder",
                Wizard().SdkPathBuffer,
                sizeof(Wizard().SdkPathBuffer)
            );

            const std::string currentPath = Wizard().SdkPathBuffer;
            const bool isValid = Paths::Onboarding::ValidateSdkPath(currentPath);
            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            if (currentPath.empty()) {
                ImGui::TextColored(
                    WizardLabelColor(),
                    "%s",
                    "Choose the folder containing your Android SDK (cmdline-tools, emulator, platform-tools, and so on)."
                );
            } else if (isValid) {
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
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float backWidth = ImGui::CalcTextSize("Back").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float continueWidth = ImGui::CalcTextSize("Continue").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton("Back", true, ImVec2(backWidth, 0))) {
                Wizard().CurrentStep = Step::SdkChoice;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - continueWidth);
            if (PositiveButton("Continue", isValid, ImVec2(continueWidth, 0))) {
                CommitSdkPath(context, currentPath);
            }

            EndWizardPanel(panel);
        }

        void BuildSdkLocateStep(Context &context) {
            PresentWizardStep([&] { DrawSdkLocateStep(context); });
        }

        void DrawSdkJdkSelectStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_jdk_select", panel, true);

            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            const std::string jdkSubtitle = StrConcat(
                "The Android command-line tools run on Java. JDK ",
                std::to_string(JDK_RECOMMENDED_MAJOR),
                " is recommended."
            );
            WizardHeading("Select a JDK", jdkSubtitle.c_str(), innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));

            const bool hasJdk = DrawJavaChoices(context);

            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float backWidth = ImGui::CalcTextSize("Back").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float continueWidth = ImGui::CalcTextSize("Continue").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton("Back", true, ImVec2(backWidth, 0))) {
                Wizard().CurrentStep = Step::SdkChoice;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - continueWidth);
            if (PositiveButton("Continue", hasJdk, ImVec2(continueWidth, 0))) {
                Wizard().CurrentStep = Step::SdkInstallRoot;
            }

            EndWizardPanel(panel);
        }

        void BuildSdkJdkSelectStep(Context &context) {
            PresentWizardStep([&] { DrawSdkJdkSelectStep(context); });
        }

        void DrawDiskRequirement(const std::string &text, const float width) {
            const float dpi = GetDpiScale();
            const float pad = WIZARD_PAD_PX * dpi;
            const float rounding = WIZARD_ROUND_PX * dpi;
            const float textWidth = std::max(1.0F, width - (pad * 2.0F));
            const ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, textWidth);
            const float boxHeight = pad + textSize.y + pad;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 boxMax(origin.x + width, origin.y + boxHeight);

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(
                origin,
                boxMax,
                ImGui::GetColorU32(HexColor(IsLightColorScheme() ? Colors::SURFACE2 : Colors::SURFACE0)),
                rounding
            );
            drawList->AddRect(origin, boxMax, ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE)), rounding);
            drawList->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize(),
                ImVec2(origin.x + pad, origin.y + pad),
                ImGui::GetColorU32(WizardLabelColor()),
                text.c_str(),
                nullptr,
                textWidth
            );
            ImGui::Dummy(ImVec2(width, boxHeight));
        }

        void DrawSdkInstallRootStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_install_card", panel, true);

            const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();
            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();

            WizardHeading("Install Android SDK", nullptr, innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            const bool hasRoot = WizardPathField(
                "##install_root",
                "Install Location",
                "SDK install folder",
                "Select where the Android SDK should be installed",
                Wizard().InstallRootBuffer,
                sizeof(Wizard().InstallRootBuffer)
            );

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            const std::string diskLine = StrConcat(
                Icons::HARD_DRIVE,
                "  Download: ~",
                FormatFileSize(release.DownloadSize),
                "  |  Free Space Recommended: 2 GB"
            );
            DrawDiskRequirement(diskLine, innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            SubtitledCheckbox(
                "##accept_sdk_license",
                &Wizard().AcceptSdkLicense,
                "I accept the Android SDK Terms and Conditions"
            );
            if (ImGui::TextLink("View License Terms...")) {
                OpenUrl(SDK_LICENSE_URL);
            }

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float backWidth = ImGui::CalcTextSize("Back").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float agreeWidth = ImGui::CalcTextSize("Agree & Install").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (8.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton("Back", true, ImVec2(backWidth, 0))) {
                Wizard().CurrentStep = Step::SdkJdkSelect;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - agreeWidth);
            const bool hasJdk = context.Host.Jdk.IsFound && context.Host.Jdk.IsValid;
            const bool canInstall = Wizard().AcceptSdkLicense && hasRoot && hasJdk && !release.DownloadUrl.empty();
            if (PositiveButton("Agree & Install", canInstall, ImVec2(agreeWidth, 0))) {
                StartBootstrap(context, Wizard().InstallRootBuffer);
                Wizard().CurrentStep = Step::SdkInstalling;
            }

            EndWizardPanel(panel);
        }

        void BuildSdkInstallRootStep(Context &context) {
            PresentWizardStep([&] { DrawSdkInstallRootStep(context); });
        }

        void DrawSdkInstallJdkStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_install_jdk", panel, true);

            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            const std::string jdkSubtitle = StrConcat(
                "Choose a JDK ",
                std::to_string(JDK_MINIMUM_MAJOR),
                " or newer so CoreDeck can run sdkmanager and avdmanager."
            );
            WizardHeading("Select a JDK folder", jdkSubtitle.c_str(), innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardPathField(
                "##jdk_home",
                "JDK location",
                "Path to a JDK 17+ installation",
                "Select a JDK directory",
                Wizard().JdkPathBuffer,
                sizeof(Wizard().JdkPathBuffer)
            );

            const std::string javaHome = Wizard().JdkPathBuffer;
            const JdkInfo candidate = InspectTypedJdk(javaHome);
            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            if (javaHome.empty()) {
                ImGui::TextColored(
                    WizardLabelColor(),
                    "%s",
                    "Pick the folder that contains bin/java. Android Studio ships one under its jbr folder."
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
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float backWidth = ImGui::CalcTextSize("Back").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float useWidth = ImGui::CalcTextSize("Use this JDK").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton("Back", true, ImVec2(backWidth, 0))) {
                Wizard().CurrentStep = Wizard().CommandLineToolsOnly ? Step::SdkInstallTools : Step::SdkInstallRoot;
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - useWidth);
            if (PositiveButton("Use this JDK", candidate.IsValid, ImVec2(useWidth, 0))) {
                Paths::Onboarding::SaveJdkPathOverride(javaHome);
                context.Host.Jdk = candidate;
                context.Host.Jdk.Source = JdkSource::Override;
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                context.Host.Manager.SetSdk(context.Host.Sdk);
                Wizard().CurrentStep = Wizard().CommandLineToolsOnly ? Step::SdkInstallTools : Step::SdkInstallRoot;
            }

            EndWizardPanel(panel);
        }

        void BuildSdkInstallJdkStep(Context &context) {
            PresentWizardStep([&] { DrawSdkInstallJdkStep(context); });
        }

        void BuildSdkInstallingStep(Context &context) {
            auto &work = context.SdkBootstrapWork;

            if (work.Future.valid() &&
                work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const bool succeeded = work.Future.get();
                work.Busy = false;

                if (work.Progress) {
                    std::scoped_lock lock(work.Progress->Mutex);
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
                std::scoped_lock lock(work.Progress->Mutex);
                stage = work.Progress->Stage;
                percent = work.Progress->Percent;
                status = work.Progress->StatusText;
                detail = work.Progress->DetailText;
            }

            bool cancelRequested = false;
            if (work.Progress) {
                std::scoped_lock lock(work.Progress->Mutex);
                cancelRequested = work.Progress->CancelRequested;
            }

            const TaskProgress task{
                .Title = Wizard().CommandLineToolsOnly ? "Installing command-line tools" : "Installing the Android SDK",
                .Subtitle = cancelRequested ? "Cancelling..." : BootstrapStageLabel(stage),
                .Fraction = percent,
                .Status = status.empty() ? nullptr : status.c_str(),
                .Detail = detail.empty() ? nullptr : detail.c_str(),
                .CancelLabel = cancelRequested ? "Cancelling..." : "Cancel",
                .CancelSizingLabel = "Cancelling...",
                .CancelEnabled = !cancelRequested,
                .CenterVertically = true,
            };
            if (TaskProgressPanel(task)) {
                if (work.Progress) {
                    std::scoped_lock lock(work.Progress->Mutex);
                    work.Progress->CancelRequested = true;
                }
            }
        }

        void DrawSdkInstallFailedStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_install_failed", panel, true);

            const auto &work = context.SdkBootstrapWork;
            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            WizardHeading("The installation didn't finish", nullptr, innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", BootstrapErrorMessage(work.LastError));
            if (!work.LastErrorDetail.empty()) {
                ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
                ImGui::TextColored(WizardLabelColor(), "%s", work.LastErrorDetail.c_str());
            }
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const char *leftLabel = Wizard().CommandLineToolsOnly ? "Close" : "Locate an SDK instead";
            const float leftWidth = ImGui::CalcTextSize(leftLabel).x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float tryWidth = ImGui::CalcTextSize("Try again").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton(leftLabel, true, ImVec2(leftWidth, 0))) {
                if (Wizard().CommandLineToolsOnly) {
                    ReturnToMain(context);
                } else {
                    Wizard().CurrentStep = Step::SdkLocate;
                }
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - tryWidth);
            if (PositiveButton("Try again", true, ImVec2(tryWidth, 0))) {
                if (work.LastError == BootstrapError::JdkRequired) {
                    if (Wizard().CommandLineToolsOnly) {
                        Wizard().CurrentStep = Step::SdkInstallJdk;
                    } else {
                        Wizard().CurrentStep = Step::SdkJdkSelect;
                    }
                } else if (Wizard().CommandLineToolsOnly) {
                    Wizard().CurrentStep = Step::SdkInstallTools;
                } else {
                    Wizard().CurrentStep = Step::SdkInstallRoot;
                }
            }

            EndWizardPanel(panel);
        }

        void BuildSdkInstallFailedStep(Context &context) {
            PresentWizardStep([&] { DrawSdkInstallFailedStep(context); });
        }

        void ReturnToMain(Context &context) {
            Wizard().CommandLineToolsOnly = false;
            Wizard().ReturnToMainOnCancel = false;
            Wizard().CurrentStep = Step::Welcome;
            context.Flow.CurrentScreen = Screen::Main;
        }

        void DrawSdkInstallToolsStep(Context &context) {
            WizardPanel panel;
            BeginWizardPanel("##sdk_install_tools", panel, true);

            const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();
            const bool platformSupported = !release.DownloadUrl.empty();
            const float dpi = GetDpiScale();
            const float innerWidth = WizardContentWidth();
            const JdkInfo &jdk = context.Host.Jdk;

            WizardHeading(
                "Install command-line tools",
                "avdmanager and sdkmanager are missing. CoreDeck will download Google's official tools into this SDK.",
                innerWidth
            );

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            ImGui::TextColored(WizardLabelColor(), "%s", "SDK location");
            ImGui::Dummy(ImVec2(0.0F, 4.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            if (context.Host.Sdk.SdkPath.empty()) {
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s", "No SDK folder is selected.");
            } else {
                ImGui::TextColored(WizardHeadingColor(), "%s", context.Host.Sdk.SdkPath.c_str());
            }
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            const std::string diskLine = StrConcat(
                Icons::HARD_DRIVE,
                "  Download: ~",
                FormatFileSize(release.DownloadSize)
            );
            DrawDiskRequirement(diskLine, innerWidth);

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerWidth);
            ImGui::TextColored(
                WizardLabelColor(),
                "%s",
                "The emulator and system images already in this folder are left in place."
            );
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            if (jdk.IsFound && jdk.IsValid) {
                const std::string javaLine = StrConcat(
                    "Using ",
                    jdk.VersionString.empty() ? jdk.JavaHome : jdk.VersionString,
                    "."
                );
                ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", javaLine.c_str());
            } else {
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "A JDK %d or newer is required. You'll be asked for one next.",
                    JDK_MINIMUM_MAJOR
                );
            }
            if (!platformSupported) {
                ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "%s",
                    "Google does not publish command-line tools for this platform."
                );
            }
            ImGui::PopTextWrapPos();

            ImGui::Dummy(ImVec2(0.0F, 12.0F * dpi));
            SubtitledCheckbox(
                "##accept_sdk_license",
                &Wizard().AcceptSdkLicense,
                "I accept the Android SDK Terms and Conditions"
            );
            if (ImGui::TextLink("View License Terms...")) {
                OpenUrl(SDK_LICENSE_URL);
            }

            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));
            WizardSeparator(innerWidth);
            ImGui::Dummy(ImVec2(0.0F, 8.0F * dpi));

            const float cancelWidth = ImGui::CalcTextSize("Cancel").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (24.0F * dpi);
            const float agreeWidth = ImGui::CalcTextSize("Agree & Install").x + (ImGui::GetStyle().FramePadding.x * 2.0F) + (8.0F * dpi);
            const float rowStart = ImGui::GetCursorPosX();
            if (PrimaryButton("Cancel", true, ImVec2(cancelWidth, 0))) {
                ReturnToMain(context);
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStart + innerWidth - agreeWidth);
            const bool canInstall = Wizard().AcceptSdkLicense && platformSupported && !context.Host.Sdk.SdkPath.empty();
            if (PositiveButton("Agree & Install", canInstall, ImVec2(agreeWidth, 0))) {
                if (!jdk.IsFound || !jdk.IsValid) {
                    Wizard().CurrentStep = Step::SdkInstallJdk;
                } else {
                    StartBootstrap(context, context.Host.Sdk.SdkPath);
                    Wizard().CurrentStep = Step::SdkInstalling;
                }
            }

            EndWizardPanel(panel);
        }

        void BuildSdkInstallToolsStep(Context &context) {
            PresentWizardStep([&] { DrawSdkInstallToolsStep(context); });
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
            BeginJdkScan();
        }
    }

    void OpenSdkSetupWizard(Context &context) {
        EnsureInitialized(context);
        Wizard().CommandLineToolsOnly = false;
        Wizard().AcceptSdkLicense = false;
        Wizard().CurrentStep = Step::SdkChoice;
        Wizard().ReturnToMainOnCancel = true;
        context.Flow.CurrentScreen = Screen::Onboarding;
    }

    void OpenCmdlineToolsInstall(Context &context) {
        EnsureInitialized(context);
        Wizard().CommandLineToolsOnly = true;
        Wizard().AcceptSdkLicense = false;
        Wizard().ReturnToMainOnCancel = true;
        Wizard().CurrentStep = Step::SdkInstallTools;
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
            case Step::SdkJdkSelect:
                BuildSdkJdkSelectStep(context);
                break;
            case Step::SdkInstallRoot:
                BuildSdkInstallRootStep(context);
                break;
            case Step::SdkInstallTools:
                BuildSdkInstallToolsStep(context);
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

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"
#include "imgui_internal.h"

#include "preferences.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/jdk.h"
#include "../../core/file_dialog.h"

namespace CoreDeck {
    namespace {
        enum class PrefsSection : uint8_t {
            General,
            AndroidSdk,
            Java,
        };

        struct SidebarItem {
            PrefsSection Section;
            const char *Icon;
            const char *Label;
        };

        constexpr SidebarItem SIDEBAR_ITEMS[] = {
            {.Section = PrefsSection::General, .Icon = Icons::GEAR, .Label = "General"},
            {.Section = PrefsSection::AndroidSdk, .Icon = Icons::MOBILE, .Label = "Android SDK"},
            {.Section = PrefsSection::Java, .Icon = Icons::COFFEE, .Label = "Java (JDK)"},
        };

        bool SidebarRow(const SidebarItem &item, const bool selected) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = ImGui::GetFrameHeight() + 6.0F;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

            ImGui::PushID(item.Label);
            const ImGuiID id = window->GetID(item.Label);
            ImGui::ItemSize(ImVec2(width, height));
            if (!ImGui::ItemAdd(bb, id)) {
                ImGui::PopID();
                return false;
            }

            bool hovered = false;
            bool held = false;
            const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

            ImU32 bg = 0;
            if (selected) {
                bg = ImGui::GetColorU32(HexColor(Colors::SURFACE3));
            } else if (hovered) {
                bg = ImGui::GetColorU32(HexColor(Colors::SURFACE2));
            }
            if (bg) {
                window->DrawList->AddRectFilled(bb.Min, bb.Max, bg);
            }

            if (selected) {
                const ImVec2 a(bb.Min.x, bb.Min.y);
                const ImVec2 b(bb.Min.x + 4.0F, bb.Max.y);
                window->DrawList->AddRectFilled(a, b, IM_COL32_WHITE);
            }

            const ImU32 textColor = ImGui::GetColorU32(selected ? HexColor(Colors::TEXT_PRIMARY) : HexColor(Colors::TEXT_SUBTLE));
            const float textY = bb.Min.y + ((height - ImGui::GetTextLineHeight()) * 0.5F);
            window->DrawList->AddText(ImVec2(bb.Min.x + 14.0F, textY), textColor, item.Icon);
            window->DrawList->AddText(ImVec2(bb.Min.x + 38.0F, textY), textColor, item.Label);

            ImGui::PopID();
            return pressed;
        }

        void SectionHeader(const char *title, const char *subtitle) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            if (subtitle && *subtitle) {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                ImGui::TextWrapped("%s", subtitle);
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        bool CheckboxRow(const char *id, const char *title, const char *tooltip, bool *value) {
            ImGui::PushID(id);
            const bool changed = ImGui::Checkbox(title, value);
            if (tooltip && *tooltip) {
                ImGui::SameLine();
                ImGui::TextColored(HexColor(Colors::TEXT_MUTED), Icons::INFO);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tooltip);
                }
            }
            ImGui::Spacing();
            ImGui::PopID();
            return changed;
        }

        void DrawGeneralSection(Context &context) {
            SectionHeader("General", "Behavior of CoreDeck while you work with AVDs.");

            if (SubtitledCheckbox(
                    "AutoScrollLogs",
                    &context.Logs.AutoScroll,
                    "Enable auto-scrolling of output logs",
                    "Keep the log view pinned to the most recent line as new output arrives."
                )) {
                PersistAppSettings(context);
            }

            ImGui::Dummy(ImVec2(0, 4));

            if (SubtitledCheckbox(
                    "ConfirmDeleteAvd",
                    &context.Prefs.ConfirmBeforeDeleteAvd,
                    "Confirm before deleting an AVD",
                    "Show a confirmation dialog when you delete a virtual device."
                )) {
                PersistAppSettings(context);
            }

            ImGui::Dummy(ImVec2(0, 4));

            if (SubtitledCheckbox(
                    "CrashReporting",
                    &context.Prefs.CrashReportingEnabled,
                    "Send crash reports and diagnostics to " COREDECK_TITLE,
                    "Share anonymous crash reports and error diagnostics (Restart Required)."
                )) {
                PersistAppSettings(context);
            }
        }

        void DrawAndroidSdkSection(Context &context, char *sdkPathBuffer, size_t bufferSize) {
            SectionHeader("Android SDK", "Where CoreDeck looks for the emulator and command-line tools.");

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted("SDK root");
            ImGui::PopStyleColor();
            const float browseWidth = Em(12.0F);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - spacing);
            ImGui::InputTextWithHint("##SdkPrefs", "Path to Android SDK", sdkPathBuffer, bufferSize);
            ImGui::SameLine();
            if (PrimaryButton("Browse...", true, ImVec2(browseWidth, 0))) {
                if (const auto picked = FileDialog::PickFolder("Select Android SDK directory", sdkPathBuffer)) {
                    strncpy(sdkPathBuffer, picked->c_str(), bufferSize - 1);
                    sdkPathBuffer[bufferSize - 1] = '\0';
                }
            }

            const std::string pathStr = sdkPathBuffer;
            const bool pathOk = Paths::Onboarding::ValidateSdkPath(pathStr);

            if (!pathStr.empty()) {
                if (pathOk) {
                    ImGui::TextColored(HexColor(Colors::POSITIVE), "Valid Android SDK path.");
                } else {
                    ImGui::TextColored(
                        HexColor(Colors::NEGATIVE),
                        "Not a valid SDK (need emulator and cmdline-tools with avdmanager)."
                    );
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                ImGui::TextUnformatted("Leave empty to auto-detect from ANDROID_HOME or default install paths.");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            if (PrimaryButton("Apply SDK Path", pathOk)) {
                Paths::Onboarding::SaveSdkPathOverride(pathStr);
                context.Host.Sdk = DetectAndroidSdk();
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                context.UI.HideInvalidSdkPathBanner = false;
                PersistAppSettings(context);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !pathOk) {
                ImGui::SetTooltip("Fix the path or validation errors before applying.");
            }

            ImGui::SameLine();
            if (PrimaryButton("Use Default Discovery", true)) {
                Paths::Onboarding::ClearSdkPathOverride();
                context.Host.Sdk = DetectAndroidSdk();
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                context.UI.HideInvalidSdkPathBanner = false;
                const std::string &p = context.Host.Sdk.SdkPath;
                strncpy(sdkPathBuffer, p.c_str(), bufferSize - 1);
                sdkPathBuffer[bufferSize - 1] = '\0';
                PersistAppSettings(context);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Forget the saved override and detect the SDK from ANDROID_HOME / default paths.");
            }
        }

        const char *JdkSourceLabel(const JdkSource source) {
            switch (source) {
                case JdkSource::Override:
                    return "Custom Path";
                case JdkSource::JavaHomeEnv:
                    return "JAVA_HOME";
                case JdkSource::Detected:
                    return "Auto-Detected";
                case JdkSource::None:
                default:
                    return "none";
            }
        }

        void DrawJdkStatus(const JdkInfo &jdk) {
            if (!jdk.IsFound) {
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "%s No JDK found at this location.", Icons::TIMES);
                return;
            }

            const char *version = jdk.VersionString.empty() ? "Java" : jdk.VersionString.c_str();
            if (jdk.IsValid) {
                ImGui::TextColored(
                    HexColor(Colors::POSITIVE),
                    "%s %s (%s)",
                    Icons::INFO,
                    version,
                    JdkSourceLabel(jdk.Source)
                );
            } else {
                ImGui::TextColored(
                    HexColor(Colors::NEGATIVE),
                    "%s %s (%s) - requires JDK %d or newer.",
                    Icons::TIMES,
                    version,
                    JdkSourceLabel(jdk.Source),
                    JDK_MINIMUM_MAJOR
                );
            }
        }

        void DrawJavaSection(Context &context, char *jdkPathBuffer, size_t bufferSize) {
            SectionHeader(
                "Java (JDK)",
                "The Android command-line tools (avdmanager, sdkmanager) run on Java and require "
                "JDK 17 or newer. Point CoreDeck at a compatible JDK if your system default is older."
            );

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted("Currently used");
            ImGui::PopStyleColor();
            if (context.Host.Jdk.IsFound) {
                DrawJdkStatus(context.Host.Jdk);
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                ImGui::TextWrapped("%s", context.Host.Jdk.JavaHome.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextColored(
                    HexColor(Colors::WARNING),
                    "%s No JDK detected. The command-line tools will use whatever 'java' is on your PATH.",
                    Icons::INFO
                );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted("JDK home");
            ImGui::PopStyleColor();
            const float browseWidth = Em(12.0F);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - spacing);
            ImGui::InputTextWithHint("##JdkPrefs", "Path to a JDK home directory", jdkPathBuffer, bufferSize);
            ImGui::SameLine();
            if (PrimaryButton("Browse...", true, ImVec2(browseWidth, 0))) {
                if (const auto picked = FileDialog::PickFolder("Select JDK home directory", jdkPathBuffer)) {
                    strncpy(jdkPathBuffer, picked->c_str(), bufferSize - 1);
                    jdkPathBuffer[bufferSize - 1] = '\0';
                }
            }

            const std::string pathStr = jdkPathBuffer;
            const bool binExists = !pathStr.empty() &&
                                   (std::filesystem::exists(
                                        Paths::JoinPaths({pathStr, "bin", "java" + Paths::GetExecutableExtension()})
                                    ) ||
                                    std::filesystem::exists(
                                        Paths::JoinPaths({pathStr, "Contents", "Home", "bin", "java" + Paths::GetExecutableExtension()})
                                    ));

            if (pathStr.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                ImGui::TextUnformatted("Leave empty to auto-detect from JAVA_HOME or standard install paths.");
                ImGui::PopStyleColor();
            } else if (!binExists) {
                ImGui::TextColored(HexColor(Colors::NEGATIVE), "No 'bin/java' found in this directory.");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                ImGui::TextUnformatted("Click Apply to validate the Java version and use this JDK.");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            if (PrimaryButton("Apply JDK Path", binExists)) {
                Paths::Onboarding::SaveJdkPathOverride(pathStr);
                context.Host.Jdk = DetectJdk();
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                strncpy(jdkPathBuffer, context.Host.Jdk.JavaHome.c_str(), bufferSize - 1);
                jdkPathBuffer[bufferSize - 1] = '\0';
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !binExists) {
                ImGui::SetTooltip("Choose a directory that contains bin/java before applying.");
            }

            ImGui::SameLine();
            if (PrimaryButton("Use Default Discovery", true)) {
                Paths::Onboarding::ClearJdkPathOverride();
                context.Host.Jdk = DetectJdk();
                ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
                context.Host.Manager.SetSdk(context.Host.Sdk);
                RefreshAvds(context);
                strncpy(jdkPathBuffer, context.Host.Jdk.JavaHome.c_str(), bufferSize - 1);
                jdkPathBuffer[bufferSize - 1] = '\0';
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Forget the saved JDK and detect it from JAVA_HOME / standard paths.");
            }
        }
    }

    void BuildPreferencesWindow(Context &context) {
        if (context.UI.ShowPreferences && !ImGui::IsPopupOpen("Preferences###CoreDeckPrefs")) {
            ImGui::OpenPopup("Preferences###CoreDeckPrefs");
        }

        static auto activeSection = PrefsSection::General;
        if (context.UI.OpenPreferencesToJava) {
            activeSection = PrefsSection::Java;
            context.UI.OpenPreferencesToJava = false;
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(EmV(100.0F, 24.0F), ImGuiCond_Appearing);

        static char sdkPathBuffer[2048];
        static char jdkPathBuffer[2048];

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (RoundedBeginPopupModal("Preferences###CoreDeckPrefs", &context.UI.ShowPreferences, WINDOW_NO_RESIZE_FLAGS)) {
            ImGui::PopStyleVar();

            if (ImGui::IsWindowAppearing()) {
                const std::string &p = context.Host.Sdk.SdkPath;
                strncpy(sdkPathBuffer, p.c_str(), sizeof(sdkPathBuffer) - 1);
                sdkPathBuffer[sizeof(sdkPathBuffer) - 1] = '\0';

                const std::string &jp = context.Host.Jdk.JavaHome;
                strncpy(jdkPathBuffer, jp.c_str(), sizeof(jdkPathBuffer) - 1);
                jdkPathBuffer[sizeof(jdkPathBuffer) - 1] = '\0';
            }

            const float sidebarWidth = Em(22.0F);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, HexColor(Colors::SURFACE0));
            ImGui::BeginChild("##PrefsSidebar", ImVec2(sidebarWidth, 0), 0);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::SetWindowFontScale(1.4F);
            const char *brand = "CoreDeck";
            const float brandW = ImGui::CalcTextSize(brand).x;
            ImGui::SetCursorPosX((sidebarWidth - brandW) * 0.5F);
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(brand);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0F);
            const char *version = "v" COREDECK_VERSION;
            const float versionW = ImGui::CalcTextSize(version).x;
            ImGui::SetCursorPosX((sidebarWidth - versionW) * 0.5F);
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::TextUnformatted(version);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 12));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
            for (const auto &item: SIDEBAR_ITEMS) {
                if (SidebarRow(item, item.Section == activeSection)) {
                    activeSection = item.Section;
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();

            // Vertical divider
            const ImVec2 popupPos = ImGui::GetWindowPos();
            const ImVec2 popupSize = ImGui::GetWindowSize();
            const float dividerX = popupPos.x + sidebarWidth;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(dividerX, popupPos.y),
                ImVec2(dividerX, popupPos.y + popupSize.y),
                ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE)),
                1.0F
            );

            ImGui::SameLine(0, 0);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0F, 12.0F));
            ImGui::BeginChild("##PrefsContent", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar();
            switch (activeSection) {
                case PrefsSection::General:
                    DrawGeneralSection(context);
                    break;
                case PrefsSection::AndroidSdk:
                    DrawAndroidSdkSection(context, sdkPathBuffer, sizeof(sdkPathBuffer));
                    break;
                case PrefsSection::Java:
                    DrawJavaSection(context, jdkPathBuffer, sizeof(jdkPathBuffer));
                    break;
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        } else {
            ImGui::PopStyleVar();
        }
    }
}
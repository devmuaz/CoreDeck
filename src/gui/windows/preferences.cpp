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
#include "../../core/file_dialog.h"

namespace CoreDeck {
    namespace {
        enum class PrefsSection {
            General,
            AndroidSdk,
        };

        struct SidebarItem {
            PrefsSection Section;
            const char *Icon;
            const char *Label;
        };

        constexpr SidebarItem SidebarItems[] = {
            {PrefsSection::General, Icons::Gear, "General"},
            {PrefsSection::AndroidSdk, Icons::Mobile, "Android SDK"},
        };

        bool SidebarRow(const SidebarItem &item, const bool selected) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = ImGui::GetFrameHeight() + 6.0f;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

            ImGui::PushID(item.Label);
            const ImGuiID id = window->GetID(item.Label);
            ImGui::ItemSize(ImVec2(width, height));
            if (!ImGui::ItemAdd(bb, id)) {
                ImGui::PopID();
                return false;
            }

            bool hovered, held;
            const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

            ImU32 bg = 0;
            if (selected) {
                bg = ImGui::GetColorU32(HexColor(Colors::Surface3));
            } else if (hovered) {
                bg = ImGui::GetColorU32(HexColor(Colors::Surface2));
            }
            if (bg) {
                window->DrawList->AddRectFilled(bb.Min, bb.Max, bg);
            }

            if (selected) {
                const ImVec2 a(bb.Min.x, bb.Min.y);
                const ImVec2 b(bb.Min.x + 4.0f, bb.Max.y);
                window->DrawList->AddRectFilled(a, b, IM_COL32_WHITE);
            }

            const ImU32 textColor = ImGui::GetColorU32(selected ? HexColor(Colors::TextPrimary) : HexColor(Colors::TextSubtle));
            const float textY = bb.Min.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
            window->DrawList->AddText(ImVec2(bb.Min.x + 14.0f, textY), textColor, item.Icon);
            window->DrawList->AddText(ImVec2(bb.Min.x + 38.0f, textY), textColor, item.Label);

            ImGui::PopID();
            return pressed;
        }

        void SectionHeader(const char *title, const char *subtitle) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextPrimary));
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            if (subtitle && *subtitle) {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextSubtle));
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
                ImGui::TextColored(HexColor(Colors::TextMuted), Icons::Info);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::Spacing();
            ImGui::PopID();
            return changed;
        }

        void DrawGeneralSection(Context &context) {
            SectionHeader("General", "Behavior of CoreDeck while you work with AVDs.");

            if (CheckboxRow(
                    "autoscroll",
                    "Auto-scroll output log",
                    "Keep the log view pinned to the most recent line as new output arrives.",
                    &context.Logs.AutoScroll
                )) {
                PersistAppSettings(context);
            }

            if (CheckboxRow(
                    "confirmdelete",
                    "Confirm before deleting an AVD",
                    "Show a confirmation dialog when you delete a virtual device.",
                    &context.Prefs.ConfirmBeforeDeleteAvd
                )) {
                PersistAppSettings(context);
            }
        }

        void DrawAndroidSdkSection(Context &context, char *sdkPathBuffer, size_t bufferSize) {
            SectionHeader("Android SDK", "Where CoreDeck looks for the emulator and command-line tools.");

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextPrimary));
            ImGui::TextUnformatted("SDK root");
            ImGui::PopStyleColor();
            constexpr float browseW = 110.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - spacing);
            ImGui::InputTextWithHint("##SdkPrefs", "Path to Android SDK", sdkPathBuffer, bufferSize);
            ImGui::SameLine();
            if (PrimaryButton("Browse...", true, ImVec2(browseW, 0))) {
                if (const auto picked = FileDialog::PickFolder("Select Android SDK directory", sdkPathBuffer)) {
                    strncpy(sdkPathBuffer, picked->c_str(), bufferSize - 1);
                    sdkPathBuffer[bufferSize - 1] = '\0';
                }
            }

            const std::string pathStr = sdkPathBuffer;
            const bool pathOk = Paths::Onboarding::ValidateSdkPath(pathStr);

            if (!pathStr.empty()) {
                if (pathOk) {
                    ImGui::TextColored(HexColor(Colors::Positive), "Valid Android SDK path.");
                } else {
                    ImGui::TextColored(
                        HexColor(Colors::Negative),
                        "Not a valid SDK (need emulator and cmdline-tools with avdmanager)."
                    );
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextSubtle));
                ImGui::TextUnformatted("Leave empty to auto-detect from ANDROID_HOME or default install paths.");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            if (PrimaryButton("Apply SDK Path", pathOk)) {
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
            if (PrimaryButton("Use Default Discovery", true)) {
                Paths::Onboarding::ClearSdkPathOverride();
                context.Host.Sdk = DetectAndroidSdk();
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
    }

    void BuildPreferencesWindow(Context &context) {
        if (context.UI.ShowPreferences && !ImGui::IsPopupOpen("Preferences###CoreDeckPrefs")) {
            ImGui::OpenPopup("Preferences###CoreDeckPrefs");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(760, 480), ImGuiCond_Appearing);

        static char sdkPathBuffer[2048];
        static auto activeSection = PrefsSection::General;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::BeginPopupModal("Preferences###CoreDeckPrefs", &context.UI.ShowPreferences, WindowNoResizeFlags)) {
            ImGui::PopStyleVar();

            if (ImGui::IsWindowAppearing()) {
                const std::string &p = context.Host.Sdk.SdkPath;
                strncpy(sdkPathBuffer, p.c_str(), sizeof(sdkPathBuffer) - 1);
                sdkPathBuffer[sizeof(sdkPathBuffer) - 1] = '\0';
            }

            constexpr float sidebarW = 200.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, HexColor(Colors::Surface0));
            ImGui::BeginChild("##PrefsSidebar", ImVec2(sidebarW, 0), false);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::SetWindowFontScale(1.4f);
            const char *brand = "CoreDeck";
            const float brandW = ImGui::CalcTextSize(brand).x;
            ImGui::SetCursorPosX((sidebarW - brandW) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextPrimary));
            ImGui::TextUnformatted(brand);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            const char *version = "v" COREDECK_VERSION;
            const float versionW = ImGui::CalcTextSize(version).x;
            ImGui::SetCursorPosX((sidebarW - versionW) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TextMuted));
            ImGui::TextUnformatted(version);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 12));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
            for (const auto &item: SidebarItems) {
                if (SidebarRow(item, item.Section == activeSection)) {
                    activeSection = item.Section;
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();

            // Vertical divider
            const ImVec2 popupPos = ImGui::GetWindowPos();
            const ImVec2 popupSize = ImGui::GetWindowSize();
            const float dividerX = popupPos.x + sidebarW;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(dividerX, popupPos.y),
                ImVec2(dividerX, popupPos.y + popupSize.y),
                ImGui::GetColorU32(HexColor(Colors::BorderSubtle)),
                1.0f
            );

            ImGui::SameLine(0, 0);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
            ImGui::BeginChild("##PrefsContent", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar();
            switch (activeSection) {
                case PrefsSection::General:
                    DrawGeneralSection(context);
                    break;
                case PrefsSection::AndroidSdk:
                    DrawAndroidSdkSection(context, sdkPathBuffer, sizeof(sdkPathBuffer));
                    break;
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        } else {
            ImGui::PopStyleVar();
        }
    }
}
//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <cstdint>

#include "imgui.h"
#include "imgui_internal.h"

#include "preferences.h"
#include "preferences_android_sdk.h"
#include "preferences_general.h"
#include "preferences_jdk.h"
#include "../theme.h"
#include "../utilities.h"
#include "../widgets.h"
#include "../../core/i18n.h"

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
            {.Section = PrefsSection::General, .Icon = Icons::GEAR, .Label = TrNoop("General")},
            {.Section = PrefsSection::AndroidSdk, .Icon = Icons::MOBILE, .Label = TrNoop("Android SDK")},
            {.Section = PrefsSection::Java, .Icon = Icons::COFFEE, .Label = TrNoop("Java (JDK)")},
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
                window->DrawList->AddRectFilled(a, b, ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)));
            }

            const ImU32 textColor = ImGui::GetColorU32(selected ? HexColor(Colors::TEXT_PRIMARY) : HexColor(Colors::TEXT_SUBTLE));
            const float textY = bb.Min.y + ((height - ImGui::GetTextLineHeight()) * 0.5F);
            window->DrawList->AddText(ImVec2(bb.Min.x + 14.0F, textY), textColor, item.Icon);
            window->DrawList->AddText(ImVec2(bb.Min.x + 38.0F, textY), textColor, Tr(item.Label));

            ImGui::PopID();
            return pressed;
        }
    }

    void PreferencesSectionHeader(const char *title, const char *subtitle) {
        ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        if (subtitle != nullptr && subtitle[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    void BuildPreferencesWindow(Context &context) {
        PollPreferencesJdkWork(context);
        PollPreferencesSdkWork(context);

        static auto activeSection = PrefsSection::General;
        if (context.UI.OpenPreferencesToJava) {
            activeSection = PrefsSection::Java;
            context.UI.OpenPreferencesToJava = false;
        }

        static char sdkPathBuffer[2048];
        static char jdkPathBuffer[2048];

        const std::string title = TrWindow("Preferences", "CoreDeckPrefs");
        if (!context.UI.ShowPreferences && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (BeginCenteredModal(title.c_str(), &context.UI.ShowPreferences, EmV(100.0F, 32.0F), WINDOW_NO_RESIZE_FLAGS)) {
            ImGui::PopStyleVar();

            if (ImGui::IsWindowAppearing()) {
                CopyToBuffer(sdkPathBuffer, sizeof(sdkPathBuffer), context.Host.Sdk.SdkPath);
                CopyToBuffer(jdkPathBuffer, sizeof(jdkPathBuffer), context.Host.Jdk.JavaHome);
                RequestPreferencesJdkScan();
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
                    DrawPreferencesGeneralSection(context);
                    break;
                case PrefsSection::AndroidSdk:
                    DrawPreferencesAndroidSdkSection(context, sdkPathBuffer, sizeof(sdkPathBuffer));
                    break;
                case PrefsSection::Java:
                    DrawPreferencesJdkSection(context, jdkPathBuffer, sizeof(jdkPathBuffer));
                    break;
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        } else {
            ImGui::PopStyleVar();
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include "imgui.h"

#include "preferences_general.h"
#include "preferences.h"
#include "../application.h"
#include "../theme.h"
#include "../widgets.h"

namespace CoreDeck {
    namespace {
        void DrawAppearancePicker(Context &context) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted("Appearance");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("Follow the system appearance, or keep CoreDeck dark or light.");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            static constexpr const char *LABELS[] = {"System", "Dark", "Light"};
            const int current = static_cast<int>(context.Prefs.Theme);
            const int selectedIndex = current >= 0 && current < IM_ARRAYSIZE(LABELS) ? current : 0;

            ImGui::SetNextItemWidth(Em(18.0F));
            ComboStyle comboStyle;
            if (ImGui::BeginCombo("##Theme", LABELS[selectedIndex])) {
                for (int i = 0; i < IM_ARRAYSIZE(LABELS); ++i) {
                    const bool selected = selectedIndex == i;
                    if (RoundedSelectable(LABELS[i], selected)) {
                        const auto preference = static_cast<ThemePreference>(i);
                        context.Prefs.Theme = preference;
                        SetThemePreference(preference);
                        RefreshThemeColors();
                        ApplyWindowChrome(context.UI.MainWindow);
                        PersistAppSettings(context);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    void DrawPreferencesGeneralSection(Context &context) {
        PreferencesSectionHeader("General", "Appearance and behavior while you work with AVDs.");

        DrawAppearancePicker(context);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

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
}

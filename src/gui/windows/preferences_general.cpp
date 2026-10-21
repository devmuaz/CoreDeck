//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include "imgui.h"

#include "preferences_general.h"
#include "preferences.h"
#include "../application.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        void DrawAppearancePicker(Context &context) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(Tr("Appearance"));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", Tr("Follow the system appearance, or keep CoreDeck dark or light."));
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const char *labels[] = {Tr("System"), Tr("Dark"), Tr("Light")};
            const int current = static_cast<int>(context.Prefs.Theme);
            int selectedIndex = current >= 0 && current < IM_ARRAYSIZE(labels) ? current : 0;

            if (RoundedCombo("##Theme", labels[selectedIndex], labels, IM_ARRAYSIZE(labels), selectedIndex, Em(18.0F))) {
                const auto preference = static_cast<ThemePreference>(selectedIndex);
                context.Prefs.Theme = preference;
                SetThemePreference(preference);
                RefreshThemeColors();
                ApplyWindowChrome(context.UI.MainWindow);
                PersistAppSettings(context);
            }
        }

        void DrawLanguagePicker(Context &context) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(Tr("Language"));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", Tr("Follow the system language, or choose one for CoreDeck."));
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const std::vector<std::string> discovered = AvailableLanguages();
            std::vector<std::string> tags;
            std::vector<std::string> labels;
            tags.emplace_back();
            labels.emplace_back(Tr("System"));
            tags.emplace_back("en");
            labels.emplace_back(LanguageEndonym("en"));
            for (const std::string &tag: discovered) {
                if (tag == "en") {
                    continue;
                }
                tags.push_back(tag);
                labels.push_back(LanguageEndonym(tag));
            }

            const std::string &current = context.Prefs.Language;
            int selectedIndex = 0;
            bool found = false;
            for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
                if (tags.at(static_cast<std::size_t>(i)) == current) {
                    selectedIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found && !current.empty()) {
                tags.push_back(current);
                labels.push_back(LanguageEndonym(current));
                selectedIndex = static_cast<int>(tags.size()) - 1;
            }

            std::vector<const char *> labelPtrs;
            labelPtrs.reserve(labels.size());
            for (const std::string &label: labels) {
                labelPtrs.push_back(label.c_str());
            }

            if (RoundedCombo(
                    "##Language",
                    labelPtrs.at(static_cast<std::size_t>(selectedIndex)),
                    labelPtrs.data(),
                    static_cast<int>(labelPtrs.size()),
                    selectedIndex,
                    Em(18.0F)
                )) {
                context.Prefs.Language = tags.at(static_cast<std::size_t>(selectedIndex));
                SetLanguage(context.Prefs.Language);
                PersistAppSettings(context);
            }
        }
    }

    void DrawPreferencesGeneralSection(Context &context) {
        PreferencesSectionHeader(Tr("General"), Tr("Appearance, language, and behavior while you work with AVDs."));

        DrawAppearancePicker(context);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawLanguagePicker(context);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (SubtitledCheckbox(
                "AutoScrollLogs",
                &context.Logs.AutoScroll,
                Tr("Enable auto-scrolling of output logs"),
                Tr("Keep the log view pinned to the most recent line as new output arrives.")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "ConfirmDeleteAvd",
                &context.Prefs.ConfirmBeforeDeleteAvd,
                Tr("Confirm before deleting an AVD"),
                Tr("Show a confirmation dialog when you delete a virtual device.")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "CrashReporting",
                &context.Prefs.CrashReportingEnabled,
                Tr("Send crash reports and diagnostics to CoreDeck"),
                Tr("Share anonymous crash reports and error diagnostics (Restart Required).")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "CheckForUpdatesOnLaunch",
                &context.Prefs.CheckForUpdatesOnLaunch,
                Tr("Check for updates on launch"),
                Tr("Look for a new CoreDeck release when the app starts.")
            )) {
            PersistAppSettings(context);
        }
    }
}

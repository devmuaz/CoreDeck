//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include "imgui.h"

#include "preferences_general.h"

#include <utility>
#include "preferences.h"
#include "../application.h"
#include "../theme.h"
#include "../widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void AppearancePicker(Context &context) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(Platform::Tr("Appearance"));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", Platform::Tr("Follow the system appearance, or keep CoreDeck dark or light."));
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const char *labels[] = {Platform::Tr("System"), Platform::Tr("Dark"), Platform::Tr("Light")};
            const int current = static_cast<int>(context.Prefs.Theme);
            int selectedIndex = current >= 0 && current < IM_ARRAYSIZE(labels) ? current : 0;

            if (RoundedCombo("##Theme", labels[selectedIndex], labels, IM_ARRAYSIZE(labels), selectedIndex, Em(18.0F))) {
                const auto preference = static_cast<App::ThemePreference>(selectedIndex);
                context.Prefs.Theme = preference;
                SetThemePreference(preference);
                RefreshThemeColors();
                ApplyWindowChrome(context.UI.MainWindow);
                PersistAppSettings(context);
            }
        }

        void LanguagePicker(Context &context) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(Platform::Tr("Language"));
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", Platform::Tr("Follow the system language, or choose one for CoreDeck."));
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const std::vector<std::string> discovered = Platform::AvailableLanguages();
            std::vector<std::string> tags;
            std::vector<std::string> labels;
            tags.emplace_back();
            labels.emplace_back(Platform::Tr("System"));
            tags.emplace_back("en");
            labels.emplace_back(Platform::LanguageEndonym("en"));
            for (const std::string &tag: discovered) {
                if (tag == "en") {
                    continue;
                }
                tags.push_back(tag);
                labels.push_back(Platform::LanguageEndonym(tag));
            }

            const std::string &current = context.Prefs.Language;
            int selectedIndex = 0;
            bool found = false;
            for (int i = 0; std::cmp_less(i, tags.size()); ++i) {
                if (tags.at(static_cast<std::size_t>(i)) == current) {
                    selectedIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found && !current.empty()) {
                tags.push_back(current);
                labels.push_back(Platform::LanguageEndonym(current));
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
                Platform::SetLanguage(context.Prefs.Language);
                PersistAppSettings(context);
            }
        }
    }

    void PreferencesGeneralSection(Context &context) {
        PreferencesSectionHeader(Platform::Tr("General"), Platform::Tr("Appearance, language, and behavior while you work with AVDs."));

        AppearancePicker(context);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        LanguagePicker(context);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (SubtitledCheckbox(
                "AutoScrollLogs",
                &context.Prefs.AutoScroll,
                Platform::Tr("Enable auto-scrolling of output logs"),
                Platform::Tr("Keep the log view pinned to the most recent line as new output arrives.")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "ConfirmDeleteAvd",
                &context.Prefs.ConfirmBeforeDeleteAvd,
                Platform::Tr("Confirm before deleting an AVD"),
                Platform::Tr("Show a confirmation dialog when you delete a virtual device.")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "CrashReporting",
                &context.Prefs.CrashReportingEnabled,
                Platform::Tr("Send crash reports and diagnostics to CoreDeck"),
                Platform::Tr("Share anonymous crash reports and error diagnostics (Restart Required).")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "CheckForUpdatesOnLaunch",
                &context.Prefs.CheckForUpdatesOnLaunch,
                Platform::Tr("Check for updates on launch"),
                Platform::Tr("Look for a new CoreDeck release when the app starts.")
            )) {
            PersistAppSettings(context);
        }

        ImGui::Dummy(ImVec2(0, 4));

        if (SubtitledCheckbox(
                "IncludeBetaReleases",
                &context.Prefs.IncludeBetaReleases,
                Platform::Tr("Include beta releases"),
                Platform::Tr("Also look for newer beta releases when checking for updates.")
            )) {
            PersistAppSettings(context);
        }
    }
}

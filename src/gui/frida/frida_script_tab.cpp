//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_script_tab.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "TextEditor.h"
#include "app/app_paths.h"
#include "app/file_dialog.h"
#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "gui/widgets/log_viewer.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        TextEditor::LanguageDefinition JavaScriptLanguage() {
            TextEditor::LanguageDefinition language = TextEditor::LanguageDefinition::C();
            language.mName = "JavaScript";
            const char *words[] = {
                "function",
                "const",
                "let",
                "var",
                "return",
                "if",
                "else",
                "new",
                "this",
                "true",
                "false",
                "null",
                "import",
                "from",
                "await",
                "async",
                "Java",
                "Interceptor",
                "Module",
                "Process",
                "Memory",
                "ptr",
                "send",
                "recv",
            };
            for (const char *word: words) {
                language.mKeywords.insert(word);
            }
            return language;
        }

        TextEditor &EditorFor(const std::string &serial) {
            static std::unordered_map<std::string, TextEditor> editors;
            const auto [it, inserted] = editors.try_emplace(serial);
            if (inserted) {
                it->second.SetLanguageDefinition(JavaScriptLanguage());
            }
            return it->second;
        }

        std::uint64_t &StampFor(const std::string &serial) {
            static std::unordered_map<std::string, std::uint64_t> stamps;
            return stamps[serial];
        }

        void LoadScriptFile(FridaDeviceSession &session, const std::filesystem::path &path) {
            const std::ifstream input(path);
            if (!input) {
                return;
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            session.Script = buffer.str();
            session.ScriptStamp += 1;
        }

        void SyncScriptEditor(FridaDeviceSession &session, TextEditor &editor) {
            std::uint64_t &stamp = StampFor(session.Serial);
            if (stamp != session.ScriptStamp) {
                editor.SetText(session.Script);
                stamp = session.ScriptStamp;
                return;
            }
            session.Script = editor.GetText();
        }

        void ScriptToolbar(Context &context, FridaDeviceSession &session, const TextEditor &editor) {
            const bool running = session.ScriptLoaded;
            const std::string runLabel = IconWithLabel(Icons::PLAY, Platform::Tr("Run"));
            const std::string stopLabel = IconWithLabel(Icons::STOP, Platform::Tr("Stop"));
            const float actionW = std::max(ActionWidth(runLabel.c_str()), ActionWidth(stopLabel.c_str()));
            const bool clicked = running ? NegativeButton(stopLabel.c_str(), true, ImVec2(actionW, 0.0F))
                                         : PrimaryButton(runLabel.c_str(), session.Attached, ImVec2(actionW, 0.0F));
            if (clicked) {
                if (running) {
                    StopFridaScript(context, session);
                } else {
                    session.Script = editor.GetText();
                    RunFridaScript(context, session);
                }
            }
            ImGui::SameLine();
            if (PickerButton(Platform::Tr("Open"))) {
                if (const auto path = App::FileDialog::PickFile(Platform::Tr("Open script"), "JavaScript", {"*.js"})) {
                    LoadScriptFile(session, *path);
                }
            }
            ImGui::SameLine();
            if (PickerButton(Platform::Tr("Save"))) {
                session.Script = editor.GetText();
                if (const auto path = App::FileDialog::SaveFile(
                        Platform::Tr("Save script"),
                        FridaScriptsDirectory(context.Workspaces.Frida),
                        "JavaScript",
                        {"*.js"}
                    )) {
                    std::ofstream output(*path, std::ios::trunc);
                    output << session.Script;
                }
            }

            std::vector<std::filesystem::path> snippetPaths;
            std::vector<std::string> snippetNames;
            std::vector<const char *> snippetLabels;
            const std::filesystem::path directory = std::filesystem::path(App::GetAssetsDirectory()) / "frida" / "snippets";
            std::error_code failure;
            if (std::filesystem::is_directory(directory, failure)) {
                for (const auto &entry: std::filesystem::directory_iterator(directory, failure)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
                    snippetPaths.push_back(entry.path());
                    snippetNames.push_back(entry.path().filename().string());
                }
            }
            snippetLabels.reserve(snippetNames.size());
            for (const std::string &name: snippetNames) {
                snippetLabels.push_back(name.c_str());
            }
            int snippet = -1;
            ImGui::SameLine();
            if (!snippetLabels.empty() &&
                RoundedCombo(
                    "##FridaSnippets",
                    Platform::Tr("Snippets"),
                    snippetLabels.data(),
                    static_cast<int>(snippetLabels.size()),
                    snippet,
                    Em(16.0F),
                    Platform::Tr("Snippets")
                ) &&
                snippet >= 0) {
                LoadScriptFile(session, snippetPaths.at(static_cast<std::size_t>(snippet)));
            }
        }

        void ApplyEditorTheme(TextEditor &editor) {
            const TextEditor::Palette &palette = IsLightColorScheme() ? TextEditor::GetLightPalette() : TextEditor::GetDarkPalette();
            if (editor.GetPalette() != palette) {
                editor.SetPalette(palette);
            }
        }

        void ScriptEditor(TextEditor &editor) {
            ApplyEditorTheme(editor);
            const float height = PaneAboveLogHeight();
            StyleVar sv;
            sv.Push(ImGuiStyleVar_ChildRounding, ImGui::GetStyle().FrameRounding);
            sv.Push(ImGuiStyleVar_ChildBorderSize, std::max(1.0F, ImGui::GetStyle().FrameBorderSize));
            editor.Render("##FridaEditor", ImVec2(0.0F, height), true);
        }

        void ScriptLog(FridaDeviceSession &session) {
            const std::string empty = std::string(Platform::Tr("No script output")) + '\n' + Platform::Tr("Run a script to see its logs here.");
            LogViewer(
                "FridaScriptLog",
                session.Console,
                session.HasConsole,
                session.ConsoleView,
                {
                    .EmptyText = empty.c_str(),
                    .AutoScroll = true,
                }
            );
        }
    }

    void BuildFridaScriptTab(Context &context, FridaDeviceSession &session) {
        TextEditor &editor = EditorFor(session.Serial);
        SyncScriptEditor(session, editor);
        ScriptToolbar(context, session, editor);
        SpaceBelowToolbar();
        ScriptEditor(editor);
        SpaceBelowToolbar();
        ScriptLog(session);
    }
}

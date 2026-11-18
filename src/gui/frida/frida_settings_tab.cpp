//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_settings_tab.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "app/frida_paths.h"
#include "frida_release.h"
#include "gui/frida/frida_controller.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"
#include "platform/strings.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        void FillIfEmpty(char *buffer, const std::size_t size, const std::string &value) {
            if (buffer == nullptr || buffer[0] != '\0' || value.empty()) {
                return;
            }
            CopyToBuffer(buffer, size, value);
        }

        void EnsureDefaultPaths(FridaState &workspace) {
            FillIfEmpty(workspace.ServerCachePath, sizeof(workspace.ServerCachePath), App::FridaServerCacheDirectory(Core::FRIDA_VERSION));
            if (workspace.ProjectPath[0] == '\0') {
                std::string error;
                FillIfEmpty(workspace.ProjectPath, sizeof(workspace.ProjectPath), App::BundledFridaProject(error));
            }
            FillIfEmpty(workspace.ScriptsPath, sizeof(workspace.ScriptsPath), App::FridaScriptsDirectory());
            if (!workspace.PathsCommitted) {
                workspace.CommittedServerCachePath = workspace.ServerCachePath;
                workspace.CommittedProjectPath = workspace.ProjectPath;
                workspace.CommittedScriptsPath = workspace.ScriptsPath;
                workspace.PathsCommitted = true;
            }
        }

        void CommitPaths(FridaState &workspace) {
            workspace.CommittedServerCachePath = workspace.ServerCachePath;
            workspace.CommittedProjectPath = workspace.ProjectPath;
            workspace.CommittedScriptsPath = workspace.ScriptsPath;
            workspace.PathsCommitted = true;
        }

        bool ValidDirectory(const char *text) {
            const std::string trimmed = Platform::Trim(text != nullptr ? text : "");
            if (trimmed.empty()) {
                return false;
            }
            const std::filesystem::path path(trimmed);
            if (!path.is_absolute()) {
                return false;
            }
            std::error_code error;
            if (!std::filesystem::exists(path, error)) {
                return true;
            }
            return std::filesystem::is_directory(path, error);
        }

        bool PathsChanged(const FridaState &workspace) {
            return workspace.CommittedServerCachePath != workspace.ServerCachePath || workspace.CommittedProjectPath != workspace.ProjectPath ||
                   workspace.CommittedScriptsPath != workspace.ScriptsPath;
        }

        bool PathsValid(const FridaState &workspace) {
            return ValidDirectory(workspace.ServerCachePath) && ValidDirectory(workspace.ProjectPath) && ValidDirectory(workspace.ScriptsPath);
        }

        void RefreshInstalledServers(FridaState &workspace) {
            workspace.InstalledServers.clear();
            std::error_code error;
            const std::filesystem::path cache(workspace.ServerCachePath);
            if (!cache.empty() && std::filesystem::is_directory(cache, error)) {
                for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(cache, error)) {
                    if (error || !entry.is_regular_file(error)) {
                        continue;
                    }
                    std::string name = entry.path().filename().string();
                    if (name.size() > 3 && name.ends_with(".xz")) {
                        continue;
                    }
                    workspace.InstalledServers.push_back(std::move(name));
                }
            }
            std::ranges::sort(workspace.InstalledServers);
            if (workspace.InstalledServer < 0 || std::cmp_greater_equal(workspace.InstalledServer, workspace.InstalledServers.size())) {
                workspace.InstalledServer = 0;
            }
            workspace.ServersScanned = true;
        }

        bool RefreshButton(const char *id) {
            return IconCircleButton(id, Icons::REFRESH, true, IconCircleButtonStyle(), Platform::Tr("Refresh"));
        }

        void ClientCard(FridaState &workspace) {
            SectionHeading(Platform::Tr("Frida"));
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float icon = IconCircleButtonDiameter();
            ImGui::SameLine(0.0F, spacing);
            const float avail = ImGui::GetContentRegionAvail().x;
            if (avail > icon) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - icon);
            }
            if (RefreshButton("##RefreshFridaServers")) {
                RefreshInstalledServers(workspace);
            }
            PropertyText(Platform::Tr("Frida version"), Core::FRIDA_VERSION);
            if (workspace.InstalledServers.empty()) {
                DisabledCombo("##FridaInstalledServers", Platform::Tr("No servers installed"), Em(22.0F));
                return;
            }
            std::vector<const char *> labels;
            labels.reserve(workspace.InstalledServers.size());
            for (const std::string &name: workspace.InstalledServers) {
                labels.push_back(name.c_str());
            }
            const float width = ComboWidth(labels.data(), static_cast<int>(labels.size()), Em(16.0F), Em(32.0F));
            RoundedCombo(
                "##FridaInstalledServers",
                labels.at(static_cast<std::size_t>(workspace.InstalledServer)),
                labels.data(),
                static_cast<int>(labels.size()),
                workspace.InstalledServer,
                width,
                Platform::Tr("Installed frida-server")
            );
        }

        void SessionCard(FridaState &workspace, FridaDeviceSession &session) {
            SectionHeading(Platform::Tr("Session"));
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const char *runtimes[] = {"QJS", "V8"};
            const char *modes[] = {Platform::Tr("Snippet"), Platform::Tr("Module")};
            const float runtimeW = ComboWidth(runtimes, IM_ARRAYSIZE(runtimes), Em(8.0F), Em(14.0F));
            const float modeW = ComboWidth(modes, IM_ARRAYSIZE(modes), Em(12.0F), Em(20.0F));
            const float room = std::max(1.0F, ImGui::GetContentRegionAvail().x - runtimeW - modeW - (spacing * 2.0F));
            const float portW = std::max(Em(8.0F), room);
            int runtime = static_cast<int>(workspace.Runtime);
            if (RoundedCombo("##FridaRuntime", runtimes[runtime], runtimes, IM_ARRAYSIZE(runtimes), runtime, runtimeW, Platform::Tr("Script runtime"))) {
                workspace.Runtime = static_cast<FridaScriptRuntime>(runtime);
                SaveFridaSettings(workspace);
                SetTabStatus(session, FridaWorkspaceTab::Settings, Tone::Positive, Platform::Tr("Script runtime saved."));
            }
            ImGui::SameLine(0.0F, spacing);
            int mode = static_cast<int>(workspace.ScriptMode);
            if (RoundedCombo("##FridaScriptMode", modes[mode], modes, IM_ARRAYSIZE(modes), mode, modeW, Platform::Tr("Script mode"))) {
                workspace.ScriptMode = static_cast<FridaScriptMode>(mode);
                SaveFridaSettings(workspace);
                SetTabStatus(session, FridaWorkspaceTab::Settings, Tone::Positive, Platform::Tr("Script mode saved."));
            }
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(portW);
            ImGui::InputTextWithHint("##FridaExistingPort", Platform::Tr("Existing server port"), workspace.ExistingPort, sizeof(workspace.ExistingPort));
            HoverTooltip(Platform::Tr("Existing server port"));
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (workspace.ExistingPort[0] == '\0') {
                    CopyToBuffer(workspace.ExistingPort, sizeof(workspace.ExistingPort), "27042");
                }
                SaveFridaSettings(workspace);
                SetTabStatus(session, FridaWorkspaceTab::Settings, Tone::Positive, Platform::Tr("Existing server port saved."));
            }
            if (PrimaryButton(Platform::Tr("Reset Agent Project"))) {
                std::string error;
                const std::string before = workspace.ProjectPath;
                const std::string bundled = App::BundledFridaProject(error);
                const std::string reset = error.empty() ? App::ResetFridaProject(before, error) : std::string{};
                const char *message = error.c_str();
                if (!reset.empty()) {
                    message = reset == bundled
                                  ? Platform::Tr("The shipped agent project is already in use.")
                                  : Platform::Tr("The agent project was copied again.");
                }
                SetTabStatus(
                    session,
                    FridaWorkspaceTab::Settings,
                    reset.empty() ? Tone::Negative : Tone::Positive,
                    message
                );
            }
        }

        void PathsCard(FridaState &workspace, FridaDeviceSession &session) {
            SectionHeading(Platform::Tr("Paths"));
            PathPicker(
                "##FridaCachePath",
                Platform::Tr("Server cache"),
                Platform::Tr("Server cache"),
                Platform::Tr("Frida server cache"),
                workspace.ServerCachePath,
                sizeof(workspace.ServerCachePath)
            );
            PathPicker(
                "##FridaProjectPath",
                Platform::Tr("Agent project"),
                Platform::Tr("Agent project"),
                Platform::Tr("Frida agent project"),
                workspace.ProjectPath,
                sizeof(workspace.ProjectPath)
            );
            PathPicker(
                "##FridaScriptsPath",
                Platform::Tr("Scripts"),
                Platform::Tr("Scripts"),
                Platform::Tr("Frida scripts"),
                workspace.ScriptsPath,
                sizeof(workspace.ScriptsPath)
            );
            const bool canSave = PathsChanged(workspace) && PathsValid(workspace);
            if (PrimaryButton(Platform::Tr("Save"), canSave)) {
                EnsureDefaultPaths(workspace);
                SaveFridaSettings(workspace);
                CommitPaths(workspace);
                workspace.ServersScanned = false;
                SetTabStatus(session, FridaWorkspaceTab::Settings, Tone::Positive, Platform::Tr("Frida paths saved."));
            }
            if (!canSave && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(
                    "%s",
                    PathsValid(workspace) ? Platform::Tr("No changes to save.") : Platform::Tr("Enter an absolute directory for each path.")
                );
            }
            ImGui::SameLine();
            if (PickerButton(Platform::Tr("Restore Defaults"))) {
                workspace.ServerCachePath[0] = '\0';
                workspace.ProjectPath[0] = '\0';
                workspace.ScriptsPath[0] = '\0';
                workspace.PathsCommitted = false;
                EnsureDefaultPaths(workspace);
                SaveFridaSettings(workspace);
                CommitPaths(workspace);
                workspace.ServersScanned = false;
                SetTabStatus(session, FridaWorkspaceTab::Settings, Tone::Positive, Platform::Tr("Frida paths restored to the defaults."));
            }
        }
    }

    void BuildFridaSettingsTab(Context &context) {
        auto &workspace = context.Workspaces.Frida;
        FridaDeviceSession &session = SelectedSession(workspace);
        EnsureDefaultPaths(workspace);
        if (!workspace.ServersScanned) {
            RefreshInstalledServers(workspace);
        }
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float column = std::max(1.0F, (avail - spacing) * 0.5F);
        FittedCardPair(
            "##FridaClient",
            [&] { ClientCard(workspace); },
            "##FridaSession",
            [&] { SessionCard(workspace, session); },
            column,
            spacing
        );
        ImGui::Spacing();
        const float pathsHeight = std::max(Em(18.0F), ImGui::GetContentRegionAvail().y);
        DetailCard("##FridaPaths", pathsHeight, [&] { PathsCard(workspace, session); });
    }
}

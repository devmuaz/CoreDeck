//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_shell_tab.h"

#include "adb_session.h"

#include "../../widgets/log_viewer.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::AdbWindow {
    namespace {
        std::string ShellEmptyText(const AdbDeviceSession &session) {
            const std::string idle = std::string(Platform::Tr("No Shell Output")) + '\n' +
                                     Platform::Tr("Run a command to see its output.");
            return OutputPlaceholder(
                session.Busy.load() && session.Task == AdbTask::Shell,
                session.HasShellOutput,
                session.ShellOutput,
                Platform::Tr("Running shell command..."),
                idle
            );
        }

        const char *ShellHistoryPreview(const AdbDeviceSession &session) {
            const char *placeholder = Platform::Tr("Recent commands");
            if (session.ShellHistorySelection.empty()) {
                return placeholder;
            }
            const auto found = std::ranges::find(session.ShellHistory, session.ShellHistorySelection);
            return found == session.ShellHistory.end() ? placeholder : found->c_str();
        }

        void RecentCommands(AdbDeviceSession &session, const float width) {
            const char *placeholder = Platform::Tr("Recent commands");
            if (session.ShellHistory.empty()) {
                DisabledCombo("##AdbShellHistory", placeholder, width);
                return;
            }

            std::vector<const char *> labels;
            labels.reserve(session.ShellHistory.size());
            int selected = -1;
            for (int index = 0; std::cmp_less(index, session.ShellHistory.size()); ++index) {
                const std::string &command = session.ShellHistory.at(static_cast<std::size_t>(index));
                labels.push_back(command.c_str());
                if (selected < 0 && command == session.ShellHistorySelection) {
                    selected = index;
                }
            }
            const char *preview = selected >= 0 ? labels.at(static_cast<std::size_t>(selected)) : placeholder;
            if (RoundedCombo("##AdbShellHistory", preview, labels.data(), static_cast<int>(labels.size()), selected, width) &&
                selected >= 0 && std::cmp_less(selected, session.ShellHistory.size())) {
                session.ShellHistorySelection = session.ShellHistory.at(static_cast<std::size_t>(selected));
                CopyToBuffer(
                    session.ShellCommand,
                    sizeof(session.ShellCommand),
                    session.ShellHistorySelection
                );
            }
        }
    }

    void ShellTab(Context &context, const Core::AdbDevice *device) {
        auto &workspace = context.AdbWork;
        auto &session = SelectedSession(workspace);
        const bool usable = DeviceUsable(device);
        ImGui::TextDisabled("%s", Platform::Tr("Runs one command at a time through adb shell. No interactive terminal is created."));

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float runW = ActionWidth(Platform::Tr("Run"));
        const float cancelW = ActionWidth(Platform::Tr("Cancel"));
        const char *recentPreview = ShellHistoryPreview(session);
        float recentW = ComboWidth(&recentPreview, 1, Em(16.0F), Em(24.0F));
        const float reserved = recentW + runW + cancelW + (spacing * 3.0F);
        float inputW = avail - reserved;
        if (inputW < Em(12.0F)) {
            recentW = std::max(Em(12.0F), avail - Em(12.0F) - runW - cancelW - (spacing * 3.0F));
            inputW = std::max(1.0F, avail - recentW - runW - cancelW - (spacing * 3.0F));
        }

        RecentCommands(session, recentW);
        ImGui::SameLine(0.0F, spacing);
        ImGui::SetNextItemWidth(inputW);
        const bool submitted = ImGui::InputTextWithHint(
            "##AdbShellCommand",
            Platform::Tr("Command, for example: getprop ro.build.version.release"),
            session.ShellCommand,
            sizeof(session.ShellCommand),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        const bool canRun = usable && !session.Busy.load() && session.ShellCommand[0] != '\0';
        ImGui::SameLine(0.0F, spacing);
        const bool run = PrimaryButton(Platform::Tr("Run"), canRun, ImVec2(runW, 0.0F));
        ImGui::SameLine(0.0F, spacing);
        if (NegativeButton(Platform::Tr("Cancel"), session.Busy.load() && session.Task == AdbTask::Shell, ImVec2(cancelW, 0.0F))) {
            session.Cancel.store(true);
        }
        if ((submitted || run) && canRun) {
            const std::string command = session.ShellCommand;
            session.SubmittedShellCommand = command;
            StartTask(
                context,
                session,
                AdbTask::Shell,
                [command](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                    return Core::RunAdbShell(sdk, serial, command, cancel);
                }
            );
        }

        ImGui::Spacing();

        const std::string empty = ShellEmptyText(session);
        LogViewer(
            "AdbShellOutput",
            session.ShellOutput,
            session.HasShellOutput,
            session.ShellView,
            {
                .EmptyText = empty.c_str(),
                .AutoScroll = false,
            }
        );
    }
}

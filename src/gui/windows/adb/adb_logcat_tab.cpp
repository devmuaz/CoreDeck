//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_logcat_tab.h"

#include "adb_session.h"

#include "../../widgets/log_viewer.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::AdbWindow {
    void LogcatTab(Context &context, const Core::AdbDevice *device) {
        auto &session = SelectedSession(context.AdbWork);
        const std::string &serial = session.Serial;
        const Core::AdbLogcatStatus status = context.Host.Adb.LogcatStatus(serial);
        const bool usable = DeviceUsable(device);
        if (session.LogcatLive && !status.Running) {
            session.LogcatLive = false;
            const std::string line = FirstStatusLine(status.Error);
            SetTabStatus(
                session,
                AdbWorkspaceTab::Logcat,
                line.empty() ? StatusBarTone::Idle : StatusBarTone::Error,
                line.empty() ? std::string(Platform::Tr("Logcat stopped...")) : line
            );
        }

        const char *priorities[] = {
            Platform::Tr("Verbose"),
            Platform::Tr("Debug"),
            Platform::Tr("Info"),
            Platform::Tr("Warning"),
            Platform::Tr("Error"),
            Platform::Tr("Fatal"),
        };
        const char *buffers[] = {
            Platform::Tr("Default"),
            Platform::Tr("Main"),
            Platform::Tr("System"),
            Platform::Tr("Crash"),
        };
        RoundedCombo(
            "##LogPriority",
            priorities[session.LogPriority],
            priorities,
            IM_ARRAYSIZE(priorities),
            session.LogPriority,
            ComboWidth(priorities, IM_ARRAYSIZE(priorities), Em(16.0F), Em(28.0F)),
            Platform::Tr("Minimum priority")
        );
        ImGui::SameLine();
        RoundedCombo(
            "##LogBuffer",
            buffers[session.LogBuffer],
            buffers,
            IM_ARRAYSIZE(buffers),
            session.LogBuffer,
            ComboWidth(buffers, IM_ARRAYSIZE(buffers), Em(16.0F), Em(28.0F)),
            Platform::Tr("Android log buffer")
        );
        ImGui::SameLine();

        if (!status.Running) {
            if (PrimaryButton(IconWithLabel(Icons::PLAY, Platform::Tr("Start")).c_str(), usable)) {
                session.LogcatOptions.MinimumPriority = static_cast<Core::AdbLogPriority>(session.LogPriority);
                switch (session.LogBuffer) {
                    case 1:
                        session.LogcatOptions.Buffers = {"main"};
                        break;
                    case 2:
                        session.LogcatOptions.Buffers = {"system"};
                        break;
                    case 3:
                        session.LogcatOptions.Buffers = {"crash"};
                        break;
                    default:
                        session.LogcatOptions.Buffers = {"main", "system", "crash"};
                        break;
                }
                if (context.Host.Adb.StartLogcat(serial, session.LogcatOptions)) {
                    session.LogcatLive = true;
                    SetTabStatus(session, AdbWorkspaceTab::Logcat, StatusBarTone::Success, Platform::Tr("Streaming logcat..."));
                } else {
                    session.LogcatLive = false;
                    const std::string line = FirstStatusLine(context.Host.Adb.LogcatStatus(serial).Error);
                    SetTabStatus(
                        session,
                        AdbWorkspaceTab::Logcat,
                        StatusBarTone::Error,
                        line.empty() ? std::string(Platform::Tr("Logcat failed to start.")) : line
                    );
                }
            }
        } else if (NegativeButton(IconWithLabel(Icons::STOP, Platform::Tr("Stop")).c_str())) {
            session.LogcatLive = false;
            SetTabStatus(session, AdbWorkspaceTab::Logcat, StatusBarTone::Idle, Platform::Tr("Logcat stopped..."));
            context.Host.Adb.StopLogcat(serial);
        }
        ImGui::SameLine();
        if (PrimaryButton(IconWithLabel(Icons::TRASH, Platform::Tr("Clear Device...")).c_str(), usable && !session.Busy.load())) {
            session.ShowClearDeviceLogDialog = true;
        }

        const auto log = context.Host.Adb.LogcatBuffer(serial);
        if (log && log->DroppedLines() > 0) {
            ImGui::TextColored(
                HexColor(Colors::WARNING),
                "%s",
                Platform::TrFormat("{0} older lines dropped", std::to_string(log->DroppedLines())).c_str()
            );
        }
        ImGui::Spacing();

        LogViewerState &view = session.LogView;
        LogViewer(
            "AdbLogcat",
            log,
            view,
            {
                .EmptyText = usable ? Platform::Tr("Start logcat to view device logs")
                                    : Platform::Tr("Select a connected device to start logcat"),
                .AutoScroll = context.Prefs.AutoScroll,
                .AllowPause = true,
                .Live = status.Running,
            }
        );
    }

    void ConfirmClearDeviceLog(Context &context) {
        auto &session = SelectedSession(context.AdbWork);
        if (!session.ShowClearDeviceLogDialog) {
            return;
        }
        const DialogResult result = SimpleDialog({
            .Id = "ConfirmClearDeviceLog",
            .IsOpen = session.ShowClearDeviceLogDialog,
            .Title = Platform::Tr("Clear Device Log?"),
            .Message = Platform::Tr("This clears Android's log buffers on the selected device."),
            .ConfirmButtonTitle = Platform::Tr("Clear"),
            .CancelButtonTitle = Platform::Tr("Cancel"),
            .Type = DialogType::Negative,
            .CloseOnConfirm = true,
        });
        if (result == DialogResult::Confirmed) {
            session.ShowClearDeviceLogDialog = false;
            StartTask(context, session, AdbTask::ClearDeviceLog, [](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                return Core::RunAdb(sdk, {"logcat", "-c"}, serial, cancel);
            });
        }
    }
}

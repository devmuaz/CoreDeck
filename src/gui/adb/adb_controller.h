//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_CONTROLLER_H
#define COREDECK_GUI_ADB_ADB_CONTROLLER_H

#include <atomic>
#include <future>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <pthread.h>
#endif

#include "gui/shell/context.h"

namespace CoreDeck::Gui::Adb {
    bool DeviceUsable(const Core::AdbDevice *device);

    std::string DeviceTitle(const Core::AdbDevice &device);

    const Core::AdbDevice *SelectedDevice(
        const Core::AdbDeviceSnapshot &snapshot,
        const std::string &serial
    );

    void SetTabStatus(AdbDeviceSession &session, AdbWorkspaceTab tab, Tone tone, std::string message);

    AdbTabStatus VisibleTabStatus(
        const AdbState &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device
    );

    AdbDeviceSession &SelectedSession(AdbState &workspace);

    bool AnySessionBusy(const AdbState &workspace);

    bool AnyDeviceLinkBusy(const AdbState &workspace);

    AdbWorkspaceTab TabForTask(AdbTask task);

    const char *BusyText(AdbTask task);

    bool IsPackageTask(AdbTask task);

    bool IsDeviceLinkTask(AdbTask task);

    std::string FirstStatusLine(const std::string &text);

    std::string OutputPlaceholder(
        bool busy,
        bool hasOutput,
        const std::string &output,
        const char *busyText,
        const std::string &idleText
    );

    void EnsureDeviceProfiles(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    void RememberWirelessDevice(Context &context, const std::string &endpoint);

    void ForgetWirelessDevice(Context &context, const std::string &endpoint);

    Core::AdbCommandResult RunPortCommand(
        const Core::SdkInfo &sdk,
        const std::string &serial,
        bool reverse,
        const std::vector<std::string> &args,
        const std::atomic<bool> *cancel
    );

    void QueueListForwards(Context &context, AdbDeviceSession &session);

    void LeaveAdb(Context &context);

    void PollTask(Context &context);

    void SyncPortRoutes(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    void EnsureSelection(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    template<typename Task>
    void StartTask(Context &context, AdbDeviceSession &session, const AdbTask task, Task run) {
        if (session.Busy.load() || context.Workspaces.Adb.CloseRequested) {
            return;
        }
        session.Cancel.store(false);
        session.Busy.store(true);
        session.Task = task;
        if (IsDeviceLinkTask(task)) {
            context.Workspaces.Adb.DevicesTone = Tone::Info;
            context.Workspaces.Adb.DevicesMessage = BusyText(task);
        }
        if (IsPackageTask(task)) {
            session.PackageOutput.clear();
            session.HasPackageOutput = false;
        }
        SetTabStatus(session, TabForTask(task), Tone::Info, BusyText(task));
        const Core::SdkInfo sdk = context.Host.Adb.Sdk();
        const std::string serial = session.Serial;
        const std::atomic<bool> *cancel = &session.Cancel;
        session.Future = std::async(
            std::launch::async,
            [sdk, serial, cancel, run = std::move(run)] {
                return run(sdk, serial, cancel);
            }
        );
    }

    template<typename Pick>
    std::optional<std::string> PickFromDialog(Context &context, Pick pick) {
#ifndef _WIN32
        sigset_t blocked{};
        sigset_t previous{};
        sigemptyset(&blocked);
        sigaddset(&blocked, SIGCHLD);
        pthread_sigmask(SIG_BLOCK, &blocked, &previous);
#endif
        context.Host.Adb.StopTracking();
        std::optional<std::string> picked = pick();
#ifndef _WIN32
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#endif
        return picked;
    }
}

#endif // COREDECK_GUI_ADB_ADB_CONTROLLER_H

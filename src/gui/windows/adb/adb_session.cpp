//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_session.h"

#include <cctype>
#include <chrono>

#include "app/app_settings.h"
#include "platform/i18n.h"
#include "platform/utilities.h"

namespace CoreDeck::Gui::AdbWindow {
    std::string DeviceTitle(const Core::AdbDevice &device) {
        if (!device.AvdName.empty()) {
            return device.AvdName;
        }
        if (!device.Model.empty()) {
            std::string model = device.Model;
            std::ranges::replace(model, '_', ' ');
            return model;
        }
        return device.Serial;
    }

    namespace {
        std::string DeviceOptionLabel(const Core::AdbDevice &device) {
            std::string title = DeviceTitle(device);
            if (title == device.Serial) {
                return title;
            }
            return title + " (" + device.Serial + ")";
        }

        AdbTabStatus &TabStatus(AdbDeviceSession &session, const AdbWorkspaceTab tab) {
            return session.TabStatus.at(static_cast<std::size_t>(tab));
        }

        const char *FailureText(const AdbTask task) {
            switch (task) {
                case AdbTask::Install:
                    return Platform::Tr("Install failed.");
                case AdbTask::Uninstall:
                    return Platform::Tr("Uninstall failed.");
                case AdbTask::ListPackages:
                    return Platform::Tr("Could not list packages.");
                case AdbTask::Push:
                    return Platform::Tr("Push failed.");
                case AdbTask::Pull:
                    return Platform::Tr("Pull failed.");
                case AdbTask::Shell:
                case AdbTask::Connect:
                case AdbTask::Disconnect:
                case AdbTask::AddForward:
                case AdbTask::RemoveForward:
                case AdbTask::AddReverse:
                case AdbTask::RemoveReverse:
                case AdbTask::ListForwards:
                case AdbTask::ListReverses:
                case AdbTask::ClearDeviceLog:
                case AdbTask::None:
                default:
                    return Platform::Tr("Command failed.");
            }
        }

        const char *SuccessText(const AdbTask task) {
            switch (task) {
                case AdbTask::Shell:
                    return Platform::Tr("Command completed.");
                case AdbTask::Install:
                    return Platform::Tr("Install completed.");
                case AdbTask::Uninstall:
                    return Platform::Tr("Uninstall completed.");
                case AdbTask::ListPackages:
                    return Platform::Tr("Package list loaded.");
                case AdbTask::Push:
                    return Platform::Tr("Push completed.");
                case AdbTask::Pull:
                    return Platform::Tr("Pull completed.");
                case AdbTask::Connect:
                case AdbTask::ConnectOverWifi:
                    return Platform::Tr("Device connected.");
                case AdbTask::Disconnect:
                    return Platform::Tr("Device disconnected.");
                case AdbTask::AddForward:
                    return Platform::Tr("Forward created.");
                case AdbTask::RemoveForward:
                    return Platform::Tr("Forward removed.");
                case AdbTask::AddReverse:
                    return Platform::Tr("Reverse created.");
                case AdbTask::RemoveReverse:
                    return Platform::Tr("Reverse removed.");
                case AdbTask::ListForwards:
                case AdbTask::ListReverses:
                    return Platform::Tr("Port mappings refreshed.");
                case AdbTask::ClearDeviceLog:
                    return Platform::Tr("Device log cleared.");
                case AdbTask::None:
                default:
                    return Platform::Tr("Command completed.");
            }
        }

        const char *IdleStatus(const AdbWorkspaceTab tab) {
            switch (tab) {
                case AdbWorkspaceTab::Logcat:
                    return Platform::Tr("Logcat is idle.");
                case AdbWorkspaceTab::Shell:
                    return Platform::Tr("Shell is idle.");
                case AdbWorkspaceTab::PackagesFiles:
                    return Platform::Tr("No package operation yet.");
                case AdbWorkspaceTab::Connections:
                    return Platform::Tr("No connection operation yet.");
            }
            return Platform::Tr("Ready.");
        }

        void RememberFront(std::vector<std::string> &entries, const std::string &value, const int limit) {
            if (value.empty()) {
                return;
            }
            std::erase(entries, value);
            entries.insert(entries.begin(), value);
            if (std::cmp_greater(entries.size(), limit)) {
                entries.resize(static_cast<std::size_t>(limit));
            }
        }

        std::vector<std::string> TrimProfileEntries(
            const std::vector<std::string> &source,
            const int limit,
            const bool wireless
        ) {
            std::vector<std::string> entries;
            for (const std::string &value: source) {
                if (value.empty() || (wireless && !Core::IsValidAdbNetworkEndpoint(value))) {
                    continue;
                }
                if (std::ranges::find(entries, value) != entries.end()) {
                    continue;
                }
                entries.push_back(value);
                if (std::cmp_greater_equal(entries.size(), limit)) {
                    break;
                }
            }
            return entries;
        }

        const App::AdbDeviceProfile *FindDeviceProfile(
            const App::AppSettings &settings,
            const std::string &serial
        ) {
            const auto found = std::ranges::find(
                settings.AdbDeviceProfiles,
                serial,
                &App::AdbDeviceProfile::Serial
            );
            return found == settings.AdbDeviceProfiles.end() ? nullptr : &*found;
        }

        void EnsureWirelessDevices(Context &context) {
            auto &workspace = context.AdbWork;
            if (workspace.WirelessLoaded) {
                return;
            }
            const App::AppSettings settings = App::LoadAppSettings();
            workspace.WirelessDevices = TrimProfileEntries(
                settings.AdbWirelessDevices,
                SAVED_WIRELESS_DEVICE_LIMIT,
                true
            );
            workspace.WirelessLoaded = true;
        }

        bool AddWirelessDevice(AdbWork &workspace, const std::string &endpoint) {
            if (!Core::IsValidAdbNetworkEndpoint(endpoint)) {
                return false;
            }
            if (std::ranges::find(workspace.WirelessDevices, endpoint) != workspace.WirelessDevices.end()) {
                return false;
            }
            RememberFront(workspace.WirelessDevices, endpoint, SAVED_WIRELESS_DEVICE_LIMIT);
            return true;
        }

        void ApplyDeviceProfile(AdbDeviceSession &session, const App::AppSettings &settings) {
            if (session.ProfileLoaded || session.Serial.empty()) {
                return;
            }

            const App::AdbDeviceProfile *profile = FindDeviceProfile(settings, session.Serial);
            if (profile) {
                session.ShellHistory = TrimProfileEntries(
                    profile->ShellCommands,
                    RECENT_SHELL_COMMAND_LIMIT,
                    false
                );
            }
            session.ProfileLoaded = true;
        }

        void BindDeviceProfile(AdbDeviceSession &session) {
            if (session.ProfileLoaded || session.Serial.empty()) {
                return;
            }
            ApplyDeviceProfile(session, App::LoadAppSettings());
        }

        void StoreDeviceProfile(App::AppSettings &settings, const AdbDeviceSession &session) {
            App::AdbDeviceProfile profile{
                .Serial = session.Serial,
                .ShellCommands = session.ShellHistory,
            };
            const auto found = std::ranges::find(
                settings.AdbDeviceProfiles,
                session.Serial,
                &App::AdbDeviceProfile::Serial
            );
            if (found == settings.AdbDeviceProfiles.end()) {
                settings.AdbDeviceProfiles.push_back(std::move(profile));
            } else {
                *found = std::move(profile);
            }
        }

        bool PersistDeviceProfiles(const Context &context) {
            App::AppSettings settings = App::LoadAppSettings();
            if (context.AdbWork.WirelessLoaded) {
                settings.AdbWirelessDevices = context.AdbWork.WirelessDevices;
            }
            for (const auto &entry: context.AdbWork.Sessions) {
                if (entry.second == nullptr ||
                    !entry.second->ProfileLoaded ||
                    entry.second->Serial.empty()) {
                    continue;
                }
                StoreDeviceProfile(settings, *entry.second);
            }
            return App::SaveAppSettings(settings);
        }

        void NoteOnlineWirelessDevices(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            EnsureWirelessDevices(context);
            std::vector<std::string> added;
            for (const Core::AdbDevice &device: snapshot.Devices) {
                if (device.State != Core::AdbDeviceState::Device) {
                    continue;
                }
                if (context.AdbWork.SuppressedWirelessDevices.contains(device.Serial)) {
                    continue;
                }
                if (AddWirelessDevice(context.AdbWork, device.Serial)) {
                    added.push_back(device.Serial);
                }
            }
            if (added.empty() || PersistDeviceProfiles(context)) {
                return;
            }
            for (const std::string &endpoint: added) {
                std::erase(context.AdbWork.WirelessDevices, endpoint);
            }
        }

        void RememberShellCommand(const Context &context, AdbDeviceSession &session, const std::string &command) {
            if (command.empty()) {
                return;
            }
            BindDeviceProfile(session);
            RememberFront(session.ShellHistory, command, RECENT_SHELL_COMMAND_LIMIT);
            PersistDeviceProfiles(context);
        }

        bool IsDeviceLinkTask(const AdbTask task) {
            return task == AdbTask::Connect || task == AdbTask::ConnectOverWifi || task == AdbTask::Disconnect;
        }

        void SetDevicesStatus(AdbWork &workspace, const StatusBarTone tone, std::string message) {
            workspace.DevicesTone = tone;
            workspace.DevicesMessage = std::move(message);
        }

        std::string LinkEndpoint(const AdbDeviceSession &session, const AdbTask task, const std::string &report) {
            if (task != AdbTask::ConnectOverWifi) {
                return session.Serial;
            }
            if (std::optional<std::string> parsed = Core::FindAdbNetworkEndpoint(report)) {
                return std::move(*parsed);
            }
            return {};
        }

        void ApplyPackageList(AdbDeviceSession &session, const Core::AdbCommandResult &result) {
            if (result.Error == Core::AdbError::Cancelled) {
                SetTabStatus(session, AdbWorkspaceTab::PackagesFiles, StatusBarTone::Warning, Platform::Tr("Command cancelled."));
                return;
            }
            if (!result.Ok()) {
                session.Packages.clear();
                std::string line = FirstStatusLine(!result.Stderr.empty() ? result.Stderr : result.Stdout);
                if (line.empty()) {
                    line = Platform::Tr("Could not list packages.");
                }
                session.PackageListError = line;
                SetTabStatus(session, AdbWorkspaceTab::PackagesFiles, StatusBarTone::Error, std::move(line));
                return;
            }
            session.Packages = Core::ParseAdbPackages(result.Stdout);
            session.PackageListError.clear();
            SetTabStatus(session, AdbWorkspaceTab::PackagesFiles, StatusBarTone::Success, Platform::Tr("Package list loaded."));
        }

        void ApplyCommandStatus(Context &context, AdbDeviceSession &session, const AdbTask task, const Core::AdbCommandResult &result) {
            if (task == AdbTask::ListPackages) {
                ApplyPackageList(session, result);
                return;
            }
            auto &workspace = context.AdbWork;
            const AdbWorkspaceTab tab = TabForTask(task);
            if (tab == AdbWorkspaceTab::PackagesFiles) {
                session.PackageOutput = result.Output();
                session.HasPackageOutput = true;
            }
            if (result.Error == Core::AdbError::Cancelled) {
                SetTabStatus(session, tab, StatusBarTone::Warning, Platform::Tr("Command cancelled."));
                if (IsDeviceLinkTask(task)) {
                    SetDevicesStatus(workspace, StatusBarTone::Warning, Platform::Tr("Command cancelled."));
                }
                return;
            }
            if (IsDeviceLinkTask(task)) {
                const std::string report = result.Output();
                const bool connecting = task != AdbTask::Disconnect;
                const bool linked = connecting
                                        ? Core::AdbConnectReportSucceeded(report)
                                        : Core::AdbDisconnectReportSucceeded(report);
                const std::string endpoint = LinkEndpoint(session, task, report);
                if (linked) {
                    if (connecting && Core::IsValidAdbNetworkEndpoint(endpoint)) {
                        workspace.SelectedSerial = endpoint;
                        workspace.HeldSerial = endpoint;
                        RememberWirelessDevice(context, endpoint);
                    } else if (task == AdbTask::Disconnect && workspace.SelectedSerial == session.Serial) {
                        workspace.SelectedSerial.clear();
                        workspace.HeldSerial.clear();
                    }
                    std::string line = FirstStatusLine(report);
                    if (line.empty()) {
                        line = SuccessText(task);
                    }
                    SetDevicesStatus(workspace, StatusBarTone::Success, line);
                    SetTabStatus(session, tab, StatusBarTone::Success, line);
                    if (connecting && endpoint != session.Serial && Core::IsValidAdbNetworkEndpoint(endpoint)) {
                        SetTabStatus(workspace.Session(endpoint), tab, StatusBarTone::Success, line);
                    }
                    return;
                }
                std::string line = FirstStatusLine(report);
                if (line.empty()) {
                    line = task == AdbTask::ConnectOverWifi
                               ? Platform::Tr("Could not read a Wi-Fi address. Join a Wi-Fi network and try again.")
                               : Platform::Tr("Command failed.");
                }
                SetDevicesStatus(workspace, StatusBarTone::Error, line);
                SetTabStatus(session, tab, StatusBarTone::Error, std::move(line));
                return;
            }
            if (result.Ok()) {
                SetTabStatus(session, tab, StatusBarTone::Success, SuccessText(task));
                return;
            }
            if (tab == AdbWorkspaceTab::PackagesFiles) {
                SetTabStatus(session, tab, StatusBarTone::Error, FailureText(task));
                return;
            }
            std::string line = FirstStatusLine(!result.Stderr.empty() ? result.Stderr : result.Stdout);
            if (line.empty()) {
                line = Platform::Tr("Command failed.");
            }
            SetTabStatus(session, tab, StatusBarTone::Error, std::move(line));
        }

        void PollSession(Context &context, AdbDeviceSession &session) {
            const auto &workspace = context.AdbWork;
            if (!session.Busy.load() || !session.Future.valid() ||
                session.Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }
            const AdbTask completed = session.Task;
            session.LastResult = session.Future.get();
            session.Busy.store(false);
            session.Task = AdbTask::None;
            if (completed == AdbTask::Shell) {
                const std::string output = session.LastResult.Output();
                if (session.LastResult.Error != Core::AdbError::Cancelled || Platform::HasVisibleText(output)) {
                    session.ShellOutput = output;
                    session.HasShellOutput = true;
                }
                if (session.LastResult.Ok()) {
                    RememberShellCommand(context, session, session.SubmittedShellCommand);
                }
                session.SubmittedShellCommand.clear();
            }
            ApplyCommandStatus(context, session, completed, session.LastResult);

            const bool followUp = !workspace.CloseRequested && !session.Cancel.load() && session.LastResult.Ok();
            if (
                followUp &&
                (completed == AdbTask::Connect ||
                 completed == AdbTask::ConnectOverWifi ||
                 completed == AdbTask::Disconnect)
            ) {
                context.Host.Adb.RequestRefresh();
            }

            if (
                followUp &&
                (completed == AdbTask::AddForward ||
                 completed == AdbTask::RemoveForward ||
                 completed == AdbTask::AddReverse ||
                 completed == AdbTask::RemoveReverse)
            ) {
                QueueListForwards(context, session);
            } else if (followUp && completed == AdbTask::ListForwards) {
                session.Forwards = Core::ParseAdbForwards(session.LastResult.Stdout);
                std::erase_if(session.Forwards, [&session](const Core::AdbForward &item) {
                    return !item.Serial.empty() && item.Serial != session.Serial;
                });
                StartTask(
                    context,
                    session,
                    AdbTask::ListReverses,
                    [](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                        return RunPortCommand(sdk, serial, true, {"--list"}, cancel);
                    }
                );
            } else if (completed == AdbTask::ListReverses && session.LastResult.Ok()) {
                session.Reverses = Core::ParseAdbForwards(session.LastResult.Stdout, true);
            }
        }

        const char *DeviceProblem(const Core::AdbDevice *device, const std::string &serial, StatusBarTone &tone) {
            if (device == nullptr) {
                if (serial.empty()) {
                    return nullptr;
                }
                tone = StatusBarTone::Warning;
                return Platform::Tr("This device is no longer reported by adb.");
            }
            if (device->State == Core::AdbDeviceState::Unauthorized) {
                tone = StatusBarTone::Warning;
                return Platform::Tr("Unlock the device and approve this computer's USB debugging key.");
            }
            if (device->State == Core::AdbDeviceState::NoPermissions) {
                tone = StatusBarTone::Warning;
                return Platform::Tr("The host does not have permission to access this USB device.");
            }
            if (device->State == Core::AdbDeviceState::Offline) {
                tone = StatusBarTone::Error;
                return Platform::Tr("adb reports this device as offline. Reconnect it and try again.");
            }
            return nullptr;
        }

        std::string PortRouteValidation(const AdbDeviceSession &session) {
            const bool started = session.ForwardLocal[0] != '\0' || session.ForwardRemote[0] != '\0';
            const bool valid = Core::IsValidAdbSocketSpec(session.ForwardLocal) &&
                               Core::IsValidAdbSocketSpec(session.ForwardRemote);
            if (started && !valid) {
                return Platform::Tr("Use adb endpoint forms such as tcp:8080 or localabstract:name.");
            }
            return {};
        }

        void ShowStatus(const StatusBarTone tone, std::string message, const std::optional<std::size_t> capturedLines) {
            if (capturedLines.has_value()) {
                message = Platform::TrFormat("{0} [{1} lines]", message, std::to_string(*capturedLines));
            }
            StatusBar(tone, message.c_str());
        }

    }

    bool DeviceUsable(const Core::AdbDevice *device) {
        return device != nullptr &&
               (device->State == Core::AdbDeviceState::Device ||
                device->State == Core::AdbDeviceState::Recovery);
    }

    const Core::AdbDevice *SelectedDevice(
        const Core::AdbDeviceSnapshot &snapshot,
        const std::string &serial
    ) {
        const auto found = std::ranges::find(snapshot.Devices, serial, &Core::AdbDevice::Serial);
        return found == snapshot.Devices.end() ? nullptr : &*found;
    }

    void SetTabStatus(AdbDeviceSession &session, const AdbWorkspaceTab tab, const StatusBarTone tone, std::string message) {
        AdbTabStatus &status = TabStatus(session, tab);
        status.Tone = tone;
        status.Message = std::move(message);
    }

    AdbDeviceSession &SelectedSession(AdbWork &workspace) {
        return workspace.Session(workspace.SelectedSerial);
    }

    bool AnySessionBusy(const AdbWork &workspace) {
        return std::ranges::any_of(workspace.Sessions, [](const auto &entry) {
            return entry.second && entry.second->Busy.load();
        });
    }

    AdbWorkspaceTab TabForTask(const AdbTask task) {
        switch (task) {
            case AdbTask::Shell:
                return AdbWorkspaceTab::Shell;
            case AdbTask::Install:
            case AdbTask::Uninstall:
            case AdbTask::ListPackages:
            case AdbTask::Push:
            case AdbTask::Pull:
                return AdbWorkspaceTab::PackagesFiles;
            case AdbTask::ClearDeviceLog:
                return AdbWorkspaceTab::Logcat;
            case AdbTask::None:
            case AdbTask::Connect:
            case AdbTask::ConnectOverWifi:
            case AdbTask::Disconnect:
            case AdbTask::AddForward:
            case AdbTask::RemoveForward:
            case AdbTask::AddReverse:
            case AdbTask::RemoveReverse:
            case AdbTask::ListForwards:
            case AdbTask::ListReverses:
            default:
                return AdbWorkspaceTab::Connections;
        }
    }

    const char *BusyText(const AdbTask task) {
        switch (task) {
            case AdbTask::Shell:
                return Platform::Tr("Running shell command...");
            case AdbTask::Install:
                return Platform::Tr("Installing package...");
            case AdbTask::Uninstall:
                return Platform::Tr("Uninstalling package...");
            case AdbTask::ListPackages:
                return Platform::Tr("Loading packages...");
            case AdbTask::Push:
                return Platform::Tr("Pushing file...");
            case AdbTask::Pull:
                return Platform::Tr("Pulling file...");
            case AdbTask::Connect:
                return Platform::Tr("Connecting...");
            case AdbTask::ConnectOverWifi:
                return Platform::Tr("Connecting over Wi-Fi...");
            case AdbTask::Disconnect:
                return Platform::Tr("Disconnecting...");
            case AdbTask::AddForward:
                return Platform::Tr("Creating forward...");
            case AdbTask::RemoveForward:
                return Platform::Tr("Removing forward...");
            case AdbTask::AddReverse:
                return Platform::Tr("Creating reverse...");
            case AdbTask::RemoveReverse:
                return Platform::Tr("Removing reverse...");
            case AdbTask::ListForwards:
            case AdbTask::ListReverses:
                return Platform::Tr("Refreshing port mappings...");
            case AdbTask::ClearDeviceLog:
                return Platform::Tr("Clearing device log...");
            case AdbTask::None:
            default:
                return Platform::Tr("Running adb command...");
        }
    }

    bool IsPackageTask(const AdbTask task) {
        return task == AdbTask::Install ||
               task == AdbTask::Uninstall ||
               task == AdbTask::Push ||
               task == AdbTask::Pull;
    }

    std::string FirstStatusLine(const std::string &text) {
        std::string line;
        for (const char c: text) {
            if (c == '\r' || c == '\n') {
                if (!line.empty()) {
                    break;
                }
                continue;
            }
            if (line.empty() && std::isspace(static_cast<unsigned char>(c))) {
                continue;
            }
            line.push_back(c);
            if (line.size() == 180) {
                break;
            }
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }
        return line;
    }

    void EnsureDeviceProfiles(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &workspace = context.AdbWork;
        const auto pending = [&](const std::string &serial) {
            if (serial.empty()) {
                return false;
            }
            const auto found = workspace.Sessions.find(serial);
            return found == workspace.Sessions.end() || found->second == nullptr || !found->second->ProfileLoaded;
        };
        bool needsLoad = pending(workspace.SelectedSerial);
        if (!needsLoad) {
            needsLoad = std::ranges::any_of(snapshot.Devices, [&](const Core::AdbDevice &device) {
                return pending(device.Serial);
            });
        }
        if (needsLoad) {
            const App::AppSettings settings = App::LoadAppSettings();
            const auto apply = [&](const std::string &serial) {
                if (!serial.empty()) {
                    ApplyDeviceProfile(workspace.Session(serial), settings);
                }
            };
            for (const Core::AdbDevice &device: snapshot.Devices) {
                apply(device.Serial);
            }
            apply(workspace.SelectedSerial);
        }
        NoteOnlineWirelessDevices(context, snapshot);
    }

    void RememberWirelessDevice(Context &context, const std::string &endpoint) {
        if (!Core::IsValidAdbNetworkEndpoint(endpoint)) {
            return;
        }
        EnsureWirelessDevices(context);
        auto &workspace = context.AdbWork;
        workspace.SuppressedWirelessDevices.erase(endpoint);
        auto &devices = workspace.WirelessDevices;
        if (!devices.empty() && devices.front() == endpoint) {
            return;
        }
        RememberFront(devices, endpoint, SAVED_WIRELESS_DEVICE_LIMIT);
        PersistDeviceProfiles(context);
    }

    void ForgetWirelessDevice(Context &context, const std::string &endpoint) {
        EnsureWirelessDevices(context);
        auto &workspace = context.AdbWork;
        workspace.SuppressedWirelessDevices.insert(endpoint);
        auto &devices = workspace.WirelessDevices;
        const std::size_t before = devices.size();
        std::erase(devices, endpoint);
        if (devices.size() != before) {
            PersistDeviceProfiles(context);
        }
    }

    Core::AdbCommandResult RunPortCommand(
        const Core::SdkInfo &sdk,
        const std::string &serial,
        const bool reverse,
        const std::vector<std::string> &args,
        const std::atomic<bool> *cancel
    ) {
        std::vector<std::string> command{reverse ? "reverse" : "forward"};
        command.insert(command.end(), args.begin(), args.end());
        return Core::RunAdb(sdk, command, serial, cancel);
    }

    void QueueListForwards(Context &context, AdbDeviceSession &session) {
        StartTask(
            context,
            session,
            AdbTask::ListForwards,
            [](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                return RunPortCommand(sdk, serial, false, {"--list"}, cancel);
            }
        );
    }

    void LeaveAdb(Context &context) {
        context.Host.Adb.StopAllLogcat();
        context.Host.Adb.StopTracking();
        auto &workspace = context.AdbWork;
        workspace.CloseRequested = false;
        workspace.ShowDevicesWindow = false;
        workspace.ShowWifiConnectHelp = false;
        workspace.HeldSerial.clear();
        workspace.DevicesTone = StatusBarTone::Idle;
        workspace.DevicesMessage.clear();
        workspace.WirelessLoaded = false;
        workspace.SuppressedWirelessDevices.clear();
        workspace.PortRoutesSerial.clear();
        workspace.PortRoutesRevision = 0;
        for (const auto &entry: workspace.Sessions) {
            if (entry.second != nullptr) {
                entry.second->ProfileLoaded = false;
            }
        }
        for (const auto &entry: workspace.Sessions) {
            if (entry.second == nullptr || !entry.second->LogcatLive) {
                continue;
            }
            entry.second->LogcatLive = false;
            SetTabStatus(*entry.second, AdbWorkspaceTab::Logcat, StatusBarTone::Idle, Platform::Tr("Logcat stopped..."));
        }
        context.UI.Workspace = WorkspacePage::Home;
    }

    void PollTask(Context &context) {
        const auto &workspace = context.AdbWork;
        std::vector<AdbDeviceSession *> sessions;
        sessions.reserve(workspace.Sessions.size());
        for (const auto &entry: workspace.Sessions) {
            if (entry.second) {
                sessions.push_back(entry.second.get());
            }
        }
        for (AdbDeviceSession *session: sessions) {
            PollSession(context, *session);
        }
        if (workspace.CloseRequested && !AnySessionBusy(workspace)) {
            LeaveAdb(context);
        }
    }

    void SyncPortRoutes(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &workspace = context.AdbWork;
        if (workspace.PortRoutesSerial == workspace.SelectedSerial &&
            workspace.PortRoutesRevision == snapshot.Revision) {
            return;
        }
        const Core::AdbDevice *device = SelectedDevice(snapshot, workspace.SelectedSerial);
        if (workspace.SelectedSerial.empty() || workspace.CloseRequested || !DeviceUsable(device)) {
            workspace.PortRoutesSerial = workspace.SelectedSerial;
            workspace.PortRoutesRevision = snapshot.Revision;
            return;
        }
        AdbDeviceSession &session = workspace.Session(workspace.SelectedSerial);
        if (session.Busy.load()) {
            workspace.PortRoutesSerial.clear();
            return;
        }
        workspace.PortRoutesSerial = workspace.SelectedSerial;
        workspace.PortRoutesRevision = snapshot.Revision;
        QueueListForwards(context, session);
    }

    void EnsureSelection(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &selected = context.AdbWork.SelectedSerial;
        const bool found = !selected.empty() && std::ranges::any_of(snapshot.Devices, [&selected](const Core::AdbDevice &device) {
            return device.Serial == selected;
        });
        if (found) {
            context.AdbWork.HeldSerial.clear();
            return;
        }
        if (snapshot.Devices.empty() ||
            (!context.AdbWork.HeldSerial.empty() && context.AdbWork.HeldSerial == selected)) {
            return;
        }
        context.AdbWork.HeldSerial.clear();
        const auto usable = std::ranges::find_if(snapshot.Devices, [](const Core::AdbDevice &device) {
            return device.State == Core::AdbDeviceState::Device;
        });
        selected = usable != snapshot.Devices.end() ? usable->Serial : snapshot.Devices.front().Serial;
    }

    float ActionWidth(const char *label) {
        const char *text = label != nullptr ? label : "";
        return ImGui::CalcTextSize(text).x +
               (ImGui::GetStyle().FramePadding.x * 2.0F) +
               (24.0F * GetDpiScale());
    }

    float ComboWidth(const char *const *labels, const int count, const float minimum, const float maximum) {
        const ImGuiStyle &style = ImGui::GetStyle();
        float width = minimum;
        for (int index = 0; index < count; ++index) {
            const char *label = labels != nullptr && labels[index] != nullptr ? labels[index] : "";
            const float item = ImGui::CalcTextSize(label).x + ImGui::GetFrameHeight() + (style.FramePadding.x * 4.0F);
            width = std::max(width, item);
        }
        return std::clamp(width, minimum, std::max(minimum, maximum));
    }

    void DisabledCombo(const char *id, const char *label, const float width) {
        int selected = 0;
        ImGui::BeginDisabled();
        RoundedCombo(id, label, &label, 1, selected, width);
        ImGui::EndDisabled();
    }

    void Checkboxes(const CheckItem *items, const int count) {
        const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float inner = ImGui::GetStyle().ItemInnerSpacing.x;
        float used = 0.0F;
        for (int index = 0; index < count; ++index) {
            const float itemWidth = 28.0F + inner + ImGui::CalcTextSize(items[index].Label).x;
            if (used > 0.0F && used + spacing + itemWidth > avail) {
                used = 0.0F;
            }
            if (used > 0.0F) {
                ImGui::SameLine(0.0F, spacing);
                used += spacing;
            }
            SubtitledCheckbox(items[index].Id, items[index].Value, items[index].Label);
            used += itemWidth;
        }
    }

    void DevicePicker(Context &context, const Core::AdbDeviceSnapshot &snapshot, const float room) {
        auto &work = context.AdbWork;
        const std::string manageLabel = IconWithLabel(Icons::WIFI, Platform::Tr("Manage Wireless Devices"));
        const float buttonW = ImGui::CalcTextSize(manageLabel.c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float right = ImGui::GetWindowContentRegionMax().x;
        const float comboRoom = std::max(1.0F, room - buttonW - spacing);
        const bool empty = snapshot.Devices.empty();

        float itemWidth = std::min(Em(24.0F), comboRoom);
        float comboWidth = itemWidth;
        std::vector<std::string> options;
        std::vector<ImVec4> dots;
        std::vector<const char *> labels;
        int selected = -1;
        std::string preview;
        const ImVec4 missingDot = HexColor(Colors::NEGATIVE);
        const ImVec4 *previewDot = nullptr;
        if (!empty) {
            options.reserve(snapshot.Devices.size());
            dots.reserve(snapshot.Devices.size());
            for (const Core::AdbDevice &device: snapshot.Devices) {
                options.push_back(DeviceOptionLabel(device));
                dots.push_back(HexColor(DeviceUsable(&device) ? Colors::POSITIVE : Colors::NEGATIVE));
            }
            labels.reserve(options.size());
            for (int index = 0; std::cmp_less(index, options.size()); ++index) {
                const auto deviceIndex = static_cast<std::size_t>(index);
                labels.push_back(options.at(deviceIndex).c_str());
                if (snapshot.Devices.at(deviceIndex).Serial == work.SelectedSerial) {
                    selected = index;
                }
            }
            preview = work.SelectedSerial;
            if (selected >= 0) {
                const auto deviceIndex = static_cast<std::size_t>(selected);
                preview = options.at(deviceIndex);
                previewDot = &dots.at(deviceIndex);
            } else if (work.SelectedSerial.empty()) {
                preview = Platform::Tr("Select a device");
            } else {
                previewDot = &missingDot;
            }
            const char *previewText = preview.c_str();
            const float slot = ComboDotSlot();
            comboWidth = std::max(
                ComboWidth(labels.data(), static_cast<int>(labels.size()), Em(24.0F), Em(48.0F)),
                ComboWidth(&previewText, 1, Em(24.0F), Em(48.0F))
            );
            itemWidth = comboWidth + slot;
            if (itemWidth > comboRoom) {
                itemWidth = std::max(1.0F, comboRoom);
                comboWidth = std::max(1.0F, itemWidth - slot);
            }
        }

        const float groupW = buttonW + spacing + itemWidth;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - groupW));
        if (PickerButton(manageLabel.c_str(), true, ImVec2(buttonW, 0.0F))) {
            work.ShowDevicesWindow = true;
        }
        ImGui::SameLine(0.0F, spacing);
        if (empty) {
            DisabledCombo("##AdbDevice", Platform::Tr("No devices"), itemWidth);
            return;
        }
        if (RoundedCombo(
                "##AdbDevice",
                preview.c_str(),
                labels.data(),
                static_cast<int>(labels.size()),
                selected,
                comboWidth,
                nullptr,
                previewDot,
                dots.data()
            ) &&
            selected >= 0 && std::cmp_less(selected, snapshot.Devices.size())) {
            work.SelectedSerial = snapshot.Devices.at(static_cast<std::size_t>(selected)).Serial;
        }
    }

    void ShowTabStatus(
        const AdbWork &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device,
        const std::optional<std::size_t> capturedLines
    ) {
        if (session.Tab == AdbWorkspaceTab::Connections) {
            if (const std::string validation = PortRouteValidation(session); !validation.empty()) {
                ShowStatus(StatusBarTone::Warning, validation, capturedLines);
                return;
            }
        }
        const AdbTabStatus &status = session.TabStatus.at(static_cast<std::size_t>(session.Tab));
        if (status.Tone == StatusBarTone::Idle && status.Message.empty()) {
            if (const std::string line = FirstStatusLine(snapshot.Error); !line.empty()) {
                ShowStatus(StatusBarTone::Warning, line, capturedLines);
                return;
            }
            StatusBarTone problemTone = StatusBarTone::Idle;
            if (const char *problem = DeviceProblem(device, workspace.SelectedSerial, problemTone)) {
                ShowStatus(problemTone, problem, capturedLines);
                return;
            }
        }
        const std::string message = status.Message.empty() ? IdleStatus(session.Tab) : status.Message;
        ShowStatus(status.Tone, message, capturedLines);
    }

    std::string OutputPlaceholder(
        const bool busy,
        const bool hasOutput,
        const std::string &output,
        const char *busyText,
        const std::string &idleText
    ) {
        if (busy && !Platform::HasVisibleText(output)) {
            return busyText;
        }
        if (hasOutput) {
            return Platform::Tr("No output.");
        }
        return idleText;
    }

}

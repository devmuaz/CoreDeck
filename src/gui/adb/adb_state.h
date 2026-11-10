//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_STATE_H
#define COREDECK_GUI_ADB_ADB_STATE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/adb/adb_client.h"
#include "core/adb/adb_devices.h"
#include "core/adb/adb_logcat.h"
#include "gui/widgets/log_viewer.h"
#include "gui/widgets/tone.h"

namespace CoreDeck::Gui {
    constexpr int SAVED_WIRELESS_DEVICE_LIMIT = 16;
    constexpr int RECENT_SHELL_COMMAND_LIMIT = 64;

    enum class AdbWorkspaceTab : uint8_t {
        Logcat,
        Shell,
        PackagesFiles,
        Connections,
    };

    enum class AdbTask : uint8_t {
        None,
        Shell,
        Install,
        Uninstall,
        ListPackages,
        Push,
        Pull,
        Connect,
        ConnectOverWifi,
        Disconnect,
        AddForward,
        RemoveForward,
        AddReverse,
        RemoveReverse,
        ListForwards,
        ListReverses,
        ClearDeviceLog,
    };

    struct AdbTabStatus {
        Gui::Tone Tone = Gui::Tone::Neutral;
        std::string Message;
    };

    struct AdbDeviceSession {
        AdbDeviceSession() = default;
        AdbDeviceSession(const AdbDeviceSession &) = delete;
        AdbDeviceSession(AdbDeviceSession &&) = delete;
        AdbDeviceSession &operator=(const AdbDeviceSession &) = delete;
        AdbDeviceSession &operator=(AdbDeviceSession &&) = delete;

        ~AdbDeviceSession() {
            Cancel.store(true);
            if (Future.valid()) {
                Future.wait();
            }
        }

        std::string Serial;
        AdbWorkspaceTab Tab = AdbWorkspaceTab::Logcat;
        std::array<AdbTabStatus, 4> TabStatus{};
        bool LogcatLive = false;
        LogViewerState LogView;
        Core::AdbLogcatOptions LogcatOptions;
        int LogPriority = 0;
        int LogBuffer = 0;

        char ShellCommand[2048] = {};
        std::vector<std::string> ShellHistory;
        std::string ShellHistorySelection;
        std::string SubmittedShellCommand;
        std::string ShellOutput;
        bool HasShellOutput = false;
        LogViewerState ShellView;
        std::string PackageOutput;
        bool HasPackageOutput = false;
        LogViewerState PackageView;

        std::string ApkPath;
        char PackageName[256] = {};
        bool ShowPackageDialog = false;
        bool PackageListRestart = false;
        Core::AdbPackageListScope PackageScope = Core::AdbPackageListScope::All;
        char PackageFilter[128] = {};
        std::vector<std::string> Packages;
        std::string PackageListError;
        char PushLocalPath[1024] = {};
        char PushRemotePath[1024] = {};
        char PullRemotePath[1024] = {};
        char PullLocalPath[1024] = {};
        bool InstallReplace = true;
        bool InstallDowngrade = false;
        bool InstallGrantPermissions = false;
        bool UninstallKeepData = false;

        char ForwardLocal[256] = {};
        char ForwardRemote[256] = {};
        bool ReverseForward = false;
        std::vector<Core::AdbForward> Forwards;
        std::vector<Core::AdbForward> Reverses;
        bool ShowUninstallDialog = false;
        bool ShowClearDeviceLogDialog = false;
        bool ProfileLoaded = false;

        std::atomic<bool> Busy{false};
        std::atomic<bool> Cancel{false};
        AdbTask Task = AdbTask::None;
        std::future<Core::AdbCommandResult> Future;
        Core::AdbCommandResult LastResult;
    };

    struct AdbState {
        AdbState() = default;
        AdbState(const AdbState &) = delete;
        AdbState(AdbState &&) = delete;
        AdbState &operator=(const AdbState &) = delete;
        AdbState &operator=(AdbState &&) = delete;

        ~AdbState() {
            for (const auto &entry: Sessions) {
                if (entry.second) {
                    entry.second->Cancel.store(true);
                }
            }
        }

        AdbDeviceSession &Session(const std::string &serial) {
            std::unique_ptr<AdbDeviceSession> &slot = Sessions[serial];
            if (!slot) {
                slot = std::make_unique<AdbDeviceSession>();
                slot->Serial = serial;
            }
            return *slot;
        }

        std::string SelectedSerial;
        std::string HeldSerial;
        std::unordered_map<std::string, std::unique_ptr<AdbDeviceSession>> Sessions;
        bool CloseRequested = false;
        bool ShowDevicesWindow = false;
        bool ShowWifiConnectHelp = false;
        bool WirelessLoaded = false;
        std::vector<std::string> WirelessDevices;
        std::unordered_set<std::string> SuppressedWirelessDevices;
        char WirelessAddress[256] = {};
        char WifiPort[8] = "5555";
        Tone DevicesTone = Tone::Neutral;
        std::string DevicesMessage;
        std::string PortRoutesSerial;
        std::uint64_t PortRoutesRevision = 0;
    };
}

#endif // COREDECK_GUI_ADB_ADB_STATE_H

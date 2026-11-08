//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_WORK_H
#define COREDECK_ADB_WORK_H

#include <array>
#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/adb_client.h"
#include "core/adb_devices.h"
#include "core/adb_logcat.h"
#include "../widgets/widgets.h"
#include "../widgets/log_viewer.h"

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
        StatusBarTone Tone = StatusBarTone::Idle;
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

    struct AdbWork {
        AdbWork() = default;
        AdbWork(const AdbWork &) = delete;
        AdbWork(AdbWork &&) = delete;
        AdbWork &operator=(const AdbWork &) = delete;
        AdbWork &operator=(AdbWork &&) = delete;

        ~AdbWork() {
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
        StatusBarTone DevicesTone = StatusBarTone::Idle;
        std::string DevicesMessage;
        std::string PortRoutesSerial;
        std::uint64_t PortRoutesRevision = 0;
    };
}

#endif // COREDECK_ADB_WORK_H

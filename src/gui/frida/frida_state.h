//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#ifndef COREDECK_GUI_FRIDA_FRIDA_STATE_H
#define COREDECK_GUI_FRIDA_FRIDA_STATE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frida/frida_server.h"
#include "gui/widgets/log_viewer.h"
#include "gui/widgets/tone.h"

namespace CoreDeck::Gui {
    inline constexpr std::size_t FRIDA_WORKSPACE_TAB_COUNT = 7;

    enum class FridaWorkspaceTab : uint8_t {
        Session,
        Script,
        Hooks,
        Java,
        Modules,
        Memory,
        Settings,
    };

    enum class FridaServerPhase : uint8_t {
        Idle,
        Working,
        Ready,
        Failed,
    };

    enum class FridaScriptMode : uint8_t {
        Snippet,
        Module,
    };

    enum class FridaScriptRuntime : uint8_t {
        Qjs,
        V8,
    };

    struct FridaTabStatus {
        ::CoreDeck::Gui::Tone Tone = ::CoreDeck::Gui::Tone::Neutral;
        std::string Message;
    };

    struct FridaProcessRow {
        std::uint32_t Pid = 0;
        std::string Name;
    };

    struct FridaModuleRow {
        std::string Name;
        std::string Base;
        std::uint64_t Size = 0;
        std::string Path;
    };

    struct FridaExportRow {
        std::string Name;
        std::string Address;
        std::string Type;
    };

    struct FridaHookRow {
        std::uint64_t Id = 0;
        std::string Target;
        std::uint64_t Hits = 0;
        std::string Last;
    };

    struct FridaHitRow {
        std::string Address;
    };

    struct FridaDeviceSession {
        FridaDeviceSession() = default;
        FridaDeviceSession(const FridaDeviceSession &) = delete;
        FridaDeviceSession &operator=(const FridaDeviceSession &) = delete;
        FridaDeviceSession(FridaDeviceSession &&) = delete;
        FridaDeviceSession &operator=(FridaDeviceSession &&) = delete;

        ~FridaDeviceSession() {
            Cancel.store(true);
            if (ServerFuture.valid()) {
                ServerFuture.wait();
            }
            if (StopFuture.valid()) {
                StopFuture.wait();
            }
        }

        std::string Serial;
        FridaWorkspaceTab Tab = FridaWorkspaceTab::Session;
        std::array<FridaTabStatus, FRIDA_WORKSPACE_TAB_COUNT> TabStatus{};

        FridaServerPhase Phase = FridaServerPhase::Idle;
        Core::FridaServerMode Mode = Core::FridaServerMode::Managed;
        bool Root = false;
        bool StopQueued = false;
        std::string Abi;
        int DevicePort = 0;
        int LocalPort = 0;
        std::atomic<bool> Cancel{false};
        std::atomic<bool> Working{false};
        std::atomic<std::uint64_t> Received{0};
        std::atomic<std::uint64_t> Total{0};
        std::atomic<const char *> ServerAsset{nullptr};
        std::future<Core::FridaEndpoint> ServerFuture;
        std::future<Core::FridaStatus> StopFuture;

        std::vector<FridaProcessRow> Processes;
        char ProcessFilter[128] = {};
        int SelectedProcess = -1;
        char SpawnPackage[256] = {};
        std::uint32_t SuspendedPid = 0;
        bool Attached = false;
        std::uint32_t AttachedPid = 0;

        std::string Script = "Java.perform(() => {\n    console.log('attached');\n});\n";
        std::uint64_t ScriptStamp = 1;
        bool ScriptLoaded = false;
        std::string Console;
        bool HasConsole = false;
        LogViewerState ConsoleView;

        char HookClass[256] = {};
        char HookMethod[256] = {};
        char HookModule[256] = {};
        char HookSymbol[256] = {};
        char HookAddress[64] = {};
        bool HookLogArgs = true;
        bool HookBacktrace = false;
        bool HookNative = false;
        std::vector<FridaHookRow> Hooks;

        char JavaQuery[128] = {};
        std::vector<std::string> JavaClasses;
        int SelectedClass = -1;
        std::vector<std::string> JavaMethods;
        int SelectedMethod = -1;

        std::vector<FridaModuleRow> Modules;
        int SelectedModule = -1;
        char ModuleFilter[128] = {};
        std::vector<FridaExportRow> Exports;

        char MemoryAddress[64] = {};
        int MemorySize = 4096;
        std::vector<std::uint8_t> Memory;
        std::vector<std::uint8_t> MemoryClean;
        std::uint64_t MemoryBase = 0;
        bool MemoryDirty = false;
        char MemoryFind[256] = {};
        bool MemoryFindUtf16 = false;
        std::vector<FridaHitRow> Hits;
        std::uint64_t PendingRead = 0;
        std::uint64_t PendingWrite = 0;
        std::uint64_t NextRequest = 1;
    };

    struct FridaState {
        FridaState() = default;
        FridaState(const FridaState &) = delete;
        FridaState &operator=(const FridaState &) = delete;
        FridaState(FridaState &&) = delete;
        FridaState &operator=(FridaState &&) = delete;

        ~FridaState() {
            for (const auto &entry: Sessions) {
                if (entry.second) {
                    entry.second->Cancel.store(true);
                }
            }
        }

        FridaDeviceSession &Session(const std::string &serial) {
            std::unique_ptr<FridaDeviceSession> &slot = Sessions[serial];
            if (!slot) {
                slot = std::make_unique<FridaDeviceSession>();
                slot->Serial = serial;
            }
            return *slot;
        }

        std::string SelectedSerial;
        bool CloseRequested = false;
        bool SettingsLoaded = false;
        std::unordered_map<std::string, std::unique_ptr<FridaDeviceSession>> Sessions;
        char ExistingPort[8] = "27042";
        FridaScriptRuntime Runtime = FridaScriptRuntime::Qjs;
        FridaScriptMode ScriptMode = FridaScriptMode::Snippet;
        char ServerCachePath[1024] = {};
        char ProjectPath[1024] = {};
        char ScriptsPath[1024] = {};
        std::string CommittedServerCachePath;
        std::string CommittedProjectPath;
        std::string CommittedScriptsPath;
        bool PathsCommitted = false;
        std::vector<std::string> InstalledServers;
        int InstalledServer = 0;
        bool ServersScanned = false;
    };
}

#endif // COREDECK_GUI_FRIDA_FRIDA_STATE_H

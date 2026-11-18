//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_controller.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#include "app/app_config.h"
#include "app/frida_paths.h"
#include "core/frida/frida_abi.h"
#include "core/frida/frida_messages.h"
#include "core/frida_release.h"
#include "gui/adb/adb_controller.h"
#include "platform/i18n.h"
#include "platform/strings.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        void CopyText(char *buffer, const std::size_t size, const std::string &value) {
            if (buffer == nullptr || size == 0) {
                return;
            }
            const std::size_t count = std::min(value.size(), size - 1);
            std::memcpy(buffer, value.data(), count);
            buffer[count] = '\0';
        }

        const char *IdleStatus(const FridaWorkspaceTab tab) {
            switch (tab) {
                case FridaWorkspaceTab::Session:
                    return Platform::Tr("Frida server is idle.");
                case FridaWorkspaceTab::Script:
                    return Platform::Tr("No script is running.");
                case FridaWorkspaceTab::Hooks:
                    return Platform::Tr("No hooks are installed.");
                case FridaWorkspaceTab::Java:
                    return Platform::Tr("Search loaded classes after attaching.");
                case FridaWorkspaceTab::Modules:
                    return Platform::Tr("Refresh modules after attaching.");
                case FridaWorkspaceTab::Memory:
                    return Platform::Tr("Refresh an address to inspect a page of memory.");
                case FridaWorkspaceTab::Settings:
                    return Platform::Tr("Frida settings.");
            }
            return Platform::Tr("Ready.");
        }

        FridaTabStatus &StatusSlot(FridaDeviceSession &session, const FridaWorkspaceTab tab) {
            return session.TabStatus.at(static_cast<std::size_t>(tab));
        }

        void AppendConsole(FridaDeviceSession &session, const std::string_view line) {
            if (line.empty()) {
                return;
            }
            if (!session.Console.empty() && session.Console.back() != '\n') {
                session.Console.push_back('\n');
            }
            Platform::AppendLimited(session.Console, line, static_cast<std::size_t>(1024U * 1024U));
            if (session.Console.empty() || session.Console.back() != '\n') {
                session.Console.push_back('\n');
            }
            session.HasConsole = true;
        }

        std::string ServerCache(const FridaState &workspace) {
            if (workspace.ServerCachePath[0] != '\0') {
                return workspace.ServerCachePath;
            }
            return App::FridaServerCacheDirectory(Core::FRIDA_VERSION);
        }

        std::string ProjectRoot(Context &context, FridaDeviceSession &session) {
            auto &workspace = context.Workspaces.Frida;
            if (workspace.ProjectPath[0] != '\0') {
                return workspace.ProjectPath;
            }
            std::string error;
            const std::string project = App::BundledFridaProject(error);
            if (project.empty()) {
                SetTabStatus(session, session.Tab, Tone::Negative, error);
            }
            return project;
        }

        bool UseV8(const FridaState &workspace) {
            return workspace.Runtime == FridaScriptRuntime::V8;
        }

        std::uint64_t PostAgent(
            Context &context,
            FridaDeviceSession &session,
            Core::FridaAgentCall call,
            const std::vector<std::uint8_t> &bytes = {}
        ) {
            const std::string project = ProjectRoot(context, session);
            if (project.empty()) {
                return 0;
            }
            call.Id = session.NextRequest++;
            context.Host.Frida.PostAgent(
                session.Serial,
                project,
                UseV8(context.Workspaces.Frida),
                call.Id,
                Core::FridaAgentRequest(call),
                bytes
            );
            return call.Id;
        }

        void NoteHookHit(FridaDeviceSession &session, const std::uint64_t id, const std::string &text) {
            for (FridaHookRow &hook: session.Hooks) {
                if (hook.Id == id) {
                    hook.Hits += 1;
                    hook.Last = text;
                    return;
                }
            }
            session.Hooks.push_back({.Id = id, .Target = text, .Hits = 1, .Last = text});
        }

        void ApplyLists(FridaDeviceSession &session, const Core::FridaEvent &event) {
            if (event.Json.find("\"modules\"") != std::string::npos) {
                session.Modules.clear();
                for (const Core::FridaModuleInfo &info: Core::ParseFridaModules(event.Json)) {
                    session.Modules.push_back({.Name = info.Name, .Base = info.Base, .Size = info.Size, .Path = info.Path});
                }
                return;
            }
            if (event.Json.find("\"exports\"") != std::string::npos) {
                session.Exports.clear();
                for (const Core::FridaExportInfo &info: Core::ParseFridaExports(event.Json)) {
                    session.Exports.push_back({.Name = info.Name, .Address = info.Address, .Type = info.Type});
                }
                return;
            }
            if (event.Json.find("\"classes\"") != std::string::npos) {
                session.JavaClasses = Core::ParseFridaNames(event.Json, "classes");
                session.SelectedClass = session.JavaClasses.empty() ? -1 : 0;
                return;
            }
            if (event.Json.find("\"methods\"") != std::string::npos) {
                session.JavaMethods = Core::ParseFridaNames(event.Json, "methods");
                session.SelectedMethod = session.JavaMethods.empty() ? -1 : 0;
                return;
            }
            if (event.Json.find("\"hits\"") != std::string::npos) {
                session.Hits.clear();
                for (const Core::FridaScanHit &hit: Core::ParseFridaHits(event.Json)) {
                    session.Hits.push_back({.Address = hit.Address});
                }
                SetTabStatus(session, FridaWorkspaceTab::Memory, Tone::Info, Platform::Tr("Scan finished."));
                return;
            }
            if (event.Json.find("\"hooks\"") != std::string::npos) {
                session.Hooks.clear();
                for (const Core::FridaHookInfo &hook: Core::ParseFridaHooks(event.Json)) {
                    session.Hooks.push_back({.Id = hook.Id, .Target = hook.Target, .Hits = 0, .Last = {}});
                }
                return;
            }
            const Core::FridaHookInfo created = Core::ParseFridaHook(event.Json);
            if (created.Id != 0) {
                session.Hooks.push_back({.Id = created.Id, .Target = created.Target, .Hits = 0, .Last = {}});
                SetTabStatus(session, FridaWorkspaceTab::Hooks, Tone::Positive, Platform::Tr("Hook installed."));
            }
        }

        void ApplyConnection(FridaDeviceSession &session, const Core::FridaEvent &event) {
            switch (event.Kind) {
                case Core::FridaEventKind::Connected:
                    session.Phase = event.Ok ? FridaServerPhase::Ready : FridaServerPhase::Failed;
                    SetTabStatus(
                        session,
                        FridaWorkspaceTab::Session,
                        event.Ok ? Tone::Positive : Tone::Negative,
                        event.Ok ? Platform::Tr("Frida is connected.") : event.Message
                    );
                    break;
                case Core::FridaEventKind::Disconnected:
                    session.Phase = FridaServerPhase::Idle;
                    session.Attached = false;
                    session.AttachedPid = 0;
                    session.SuspendedPid = 0;
                    session.ScriptLoaded = false;
                    SetTabStatus(session, FridaWorkspaceTab::Session, Tone::Neutral, Platform::Tr("Frida disconnected."));
                    break;
                case Core::FridaEventKind::Attached:
                    session.Attached = event.Ok;
                    session.AttachedPid = event.Ok ? event.Pid : 0;
                    SetTabStatus(
                        session,
                        FridaWorkspaceTab::Session,
                        event.Ok ? Tone::Positive : Tone::Negative,
                        event.Ok ? Platform::Tr("Attached.") : event.Message
                    );
                    break;
                case Core::FridaEventKind::Spawned:
                    session.SuspendedPid = event.Pid;
                    session.Attached = event.Ok;
                    session.AttachedPid = event.Ok ? event.Pid : 0;
                    SetTabStatus(
                        session,
                        FridaWorkspaceTab::Session,
                        event.Ok ? Tone::Warning : Tone::Negative,
                        event.Ok ? Platform::Tr("Spawned and suspended. Load a script, then resume.") : event.Message
                    );
                    break;
                case Core::FridaEventKind::Resumed:
                    if (event.Ok) {
                        session.SuspendedPid = 0;
                    }
                    SetTabStatus(
                        session,
                        FridaWorkspaceTab::Session,
                        event.Ok ? Tone::Positive : Tone::Negative,
                        event.Ok ? Platform::Tr("Process resumed.") : event.Message
                    );
                    break;
                case Core::FridaEventKind::Detached:
                    session.Attached = false;
                    session.AttachedPid = 0;
                    session.SuspendedPid = 0;
                    session.ScriptLoaded = false;
                    SetTabStatus(session, FridaWorkspaceTab::Session, event.Ok ? Tone::Neutral : Tone::Warning, event.Message);
                    break;
                default:
                    break;
            }
        }

        void ApplyEvent(Context &context, FridaDeviceSession &session, const Core::FridaEvent &event) {
            if (event.Kind == Core::FridaEventKind::Processes) {
                session.Processes.clear();
                session.SelectedProcess = -1;
                for (const Core::FridaProcessInfo &info: event.Processes) {
                    session.Processes.push_back({.Pid = info.Pid, .Name = info.Name});
                }
                if (!event.Ok) {
                    if (!session.Attached) {
                        session.Phase = FridaServerPhase::Failed;
                        context.Host.Frida.ForgetServer(session.Serial);
                    }
                    SetTabStatus(session, FridaWorkspaceTab::Session, Tone::Negative, event.Message);
                }
                return;
            }
            if (event.Kind == Core::FridaEventKind::Script) {
                session.ScriptLoaded = event.Ok && event.Message != "Script stopped.";
                AppendConsole(session, event.Message);
                SetTabStatus(session, FridaWorkspaceTab::Script, event.Ok ? Tone::Positive : Tone::Negative, event.Message);
                return;
            }
            if (event.Kind == Core::FridaEventKind::Console) {
                AppendConsole(session, event.Message);
                return;
            }
            if (event.Kind == Core::FridaEventKind::Hook) {
                NoteHookHit(session, event.RequestId, event.Message);
                AppendConsole(session, event.Message);
                return;
            }
            if (event.Kind == Core::FridaEventKind::Rpc) {
                if (!event.Ok) {
                    AppendConsole(session, event.Message);
                    SetTabStatus(session, session.Tab, Tone::Negative, event.Message);
                    return;
                }
                if (event.RequestId == session.PendingRead && !event.Bytes.empty()) {
                    session.Memory = event.Bytes;
                    session.MemoryClean = event.Bytes;
                    session.MemoryDirty = false;
                    session.PendingRead = 0;
                    SetTabStatus(session, FridaWorkspaceTab::Memory, Tone::Positive, Platform::Tr("Memory page loaded."));
                    return;
                }
                if (event.RequestId == session.PendingWrite) {
                    session.MemoryClean = session.Memory;
                    session.MemoryDirty = false;
                    session.PendingWrite = 0;
                    SetTabStatus(session, FridaWorkspaceTab::Memory, Tone::Positive, Platform::Tr("Memory written."));
                    return;
                }
                ApplyLists(session, event);
                return;
            }
            if (event.Kind == Core::FridaEventKind::Failed) {
                AppendConsole(session, event.Message);
                SetTabStatus(session, session.Tab, Tone::Negative, event.Message);
                return;
            }
            if (event.Kind == Core::FridaEventKind::Connected && event.Ok) {
                context.Host.Frida.RefreshProcesses(session.Serial);
            } else if (event.Kind == Core::FridaEventKind::Connected) {
                context.Host.Frida.ForgetServer(session.Serial);
                session.LocalPort = 0;
            }
            ApplyConnection(session, event);
        }

        void PollServer(Context &context, FridaDeviceSession &session) {
            if (session.ServerFuture.valid() && session.ServerFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const Core::FridaEndpoint endpoint = session.ServerFuture.get();
                session.Working.store(false);
                session.Abi = endpoint.Abi;
                session.Root = endpoint.Root;
                session.DevicePort = endpoint.DevicePort;
                session.LocalPort = endpoint.LocalPort;
                session.Mode = endpoint.Managed ? Core::FridaServerMode::Managed : Core::FridaServerMode::Existing;
                if (!endpoint.Ok()) {
                    session.Phase = FridaServerPhase::Failed;
                    SetTabStatus(session, FridaWorkspaceTab::Session, Tone::Negative, endpoint.Status.Message);
                    return;
                }
                Core::FridaTrackedServer tracked;
                tracked.Sdk = context.Host.Sdk;
                tracked.Serial = session.Serial;
                tracked.Mode = session.Mode;
                tracked.LocalPort = endpoint.LocalPort;
                context.Host.Frida.NoteServer(tracked);
                context.Host.Frida.Connect(session.Serial, endpoint.Address);
            }
            if (session.StopFuture.valid() && session.StopFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const Core::FridaStatus status = session.StopFuture.get();
                session.Working.store(false);
                session.StopQueued = false;
                session.LocalPort = 0;
                context.Host.Frida.ForgetServer(session.Serial);
                context.Host.Frida.Disconnect(session.Serial);
                SetTabStatus(session, FridaWorkspaceTab::Session, status.Ok() ? Tone::Neutral : Tone::Warning, status.Message);
            }
        }

        void BeginServer(Context &context, FridaDeviceSession &session, const Core::FridaServerMode mode) {
            auto &workspace = context.Workspaces.Frida;
            if (session.Working.load()) {
                return;
            }
            context.Host.Frida.Start();
            session.Cancel.store(false);
            session.Received.store(0);
            session.Total.store(0);
            session.ServerAsset.store(nullptr);
            session.Working.store(true);
            session.Phase = FridaServerPhase::Working;
            session.Mode = mode;
            SetTabStatus(
                session,
                FridaWorkspaceTab::Session,
                Tone::Info,
                mode == Core::FridaServerMode::Managed ? Platform::Tr("Starting frida-server...") : Platform::Tr("Attaching to the existing server...")
            );
            const Core::SdkInfo sdk = context.Host.Sdk;
            const std::string serial = session.Serial;
            const std::string cache = ServerCache(workspace);
            int devicePort = Core::FRIDA_EXISTING_PORT;
            if (mode == Core::FridaServerMode::Existing) {
                try {
                    devicePort = std::stoi(workspace.ExistingPort);
                } catch (...) {
                    devicePort = Core::FRIDA_EXISTING_PORT;
                }
            }
            session.ServerFuture = std::async(std::launch::async, [=, &session] {
                Core::FridaServerRequest request;
                request.Sdk = sdk;
                request.Serial = serial;
                request.CacheDirectory = cache;
                request.Mode = mode;
                request.DevicePort = devicePort;
                request.Cancel = &session.Cancel;
                request.Received = &session.Received;
                request.Total = &session.Total;
                request.AssetLabel = &session.ServerAsset;
                return Core::StartFridaServer(request);
            });
        }

        void LeaveFrida(Context &context) {
            context.Workspaces.Frida.CloseRequested = false;
            context.Shell.Workspace = WorkspacePage::Home;
        }
    }

    FridaDeviceSession &SelectedSession(FridaState &workspace) {
        return workspace.Session(workspace.SelectedSerial);
    }

    void SetTabStatus(FridaDeviceSession &session, const FridaWorkspaceTab tab, const Tone tone, std::string message) {
        FridaTabStatus &status = StatusSlot(session, tab);
        status.Tone = tone;
        status.Message = std::move(message);
    }

    FridaTabStatus VisibleTabStatus(
        const FridaState &workspace,
        const FridaDeviceSession &session,
        const Core::AdbDevice *device
    ) {
        const FridaTabStatus &status = session.TabStatus.at(static_cast<std::size_t>(session.Tab));
        if (session.Tab != FridaWorkspaceTab::Settings && (status.Tone == Tone::Neutral && status.Message.empty())) {
            if (workspace.SelectedSerial.empty() || device == nullptr) {
                return {.Tone = Tone::Warning, .Message = Platform::Tr("Select a device.")};
            }
            if (!Adb::DeviceUsable(device)) {
                return {.Tone = Tone::Negative, .Message = Platform::Tr("This device is not ready for Frida.")};
            }
        }
        if (status.Message.empty()) {
            return {.Tone = Tone::Neutral, .Message = IdleStatus(session.Tab)};
        }
        return status;
    }

    bool AnySessionWorking(const FridaState &workspace) {
        return std::ranges::any_of(workspace.Sessions, [](const auto &entry) {
            return entry.second != nullptr && entry.second->Working.load();
        });
    }

    void LoadFridaSettings(FridaState &workspace) {
        if (workspace.SettingsLoaded) {
            return;
        }
        const App::AppConfig config = App::LoadAppConfig();
        const std::string &storedPort = config.Frida->ExistingPort.value();
        const std::string port = storedPort.empty() ? "27042" : storedPort;
        CopyText(workspace.ExistingPort, sizeof(workspace.ExistingPort), port);
        CopyText(workspace.ServerCachePath, sizeof(workspace.ServerCachePath), config.Frida->ServerCachePath.value());
        CopyText(workspace.ProjectPath, sizeof(workspace.ProjectPath), config.Frida->ProjectPath.value());
        CopyText(workspace.ScriptsPath, sizeof(workspace.ScriptsPath), config.Frida->ScriptsPath.value());
        if (config.Frida->Runtime.value() == static_cast<int>(FridaScriptRuntime::V8)) {
            workspace.Runtime = FridaScriptRuntime::V8;
        }
        if (config.Frida->Mode.value() == static_cast<int>(FridaScriptMode::Module)) {
            workspace.ScriptMode = FridaScriptMode::Module;
        }
        workspace.SettingsLoaded = true;
    }

    void SaveFridaSettings(const FridaState &workspace) {
        App::UpdateAppConfig([&workspace](App::AppConfig &config) {
            config.Frida->ExistingPort = workspace.ExistingPort;
            config.Frida->ServerCachePath = workspace.ServerCachePath;
            config.Frida->ProjectPath = workspace.ProjectPath;
            config.Frida->ScriptsPath = workspace.ScriptsPath;
            config.Frida->Runtime = static_cast<int>(workspace.Runtime);
            config.Frida->Mode = static_cast<int>(workspace.ScriptMode);
        });
    }

    void EnsureSelection(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &workspace = context.Workspaces.Frida;
        if (snapshot.Devices.empty()) {
            workspace.SelectedSerial.clear();
            return;
        }
        const auto found = std::ranges::find(snapshot.Devices, workspace.SelectedSerial, &Core::AdbDevice::Serial);
        if (found != snapshot.Devices.end() && Adb::DeviceUsable(&*found)) {
            return;
        }
        const auto usable = std::ranges::find_if(snapshot.Devices, [](const Core::AdbDevice &device) { return Adb::DeviceUsable(&device); });
        workspace.SelectedSerial = usable != snapshot.Devices.end() ? usable->Serial : snapshot.Devices.front().Serial;
    }

    void PollFrida(Context &context) {
        auto &workspace = context.Workspaces.Frida;
        LoadFridaSettings(workspace);
        if (workspace.CloseRequested) {
            for (const auto &entry: workspace.Sessions) {
                if (entry.second != nullptr && entry.second->Phase == FridaServerPhase::Ready && !entry.second->StopQueued) {
                    StopFridaSession(context, *entry.second);
                }
            }
        }
        for (const auto &entry: workspace.Sessions) {
            if (entry.second != nullptr) {
                PollServer(context, *entry.second);
            }
        }
        for (const Core::FridaEvent &event: context.Host.Frida.Drain()) {
            const auto found = workspace.Sessions.find(event.Serial);
            if (found == workspace.Sessions.end() || found->second == nullptr) {
                continue;
            }
            ApplyEvent(context, *found->second, event);
        }
        if (workspace.CloseRequested && !AnySessionWorking(workspace)) {
            const bool stillUp = std::ranges::any_of(workspace.Sessions, [](const auto &entry) {
                return entry.second != nullptr && entry.second->Phase == FridaServerPhase::Ready && !entry.second->StopQueued;
            });
            if (!stillUp) {
                LeaveFrida(context);
            }
        }
    }

    void StartManagedFrida(Context &context, FridaDeviceSession &session) {
        BeginServer(context, session, Core::FridaServerMode::Managed);
    }

    void UseExistingFrida(Context &context, FridaDeviceSession &session) {
        BeginServer(context, session, Core::FridaServerMode::Existing);
    }

    void StopFridaSession(const Context &context, FridaDeviceSession &session) {
        if (session.Working.load()) {
            session.Cancel.store(true);
            return;
        }
        session.StopQueued = true;
        session.Working.store(true);
        session.Cancel.store(false);
        const Core::SdkInfo sdk = context.Host.Sdk;
        const std::string serial = session.Serial;
        const Core::FridaServerMode mode = session.Mode;
        const int localPort = session.LocalPort;
        session.StopFuture = std::async(std::launch::async, [=, &session] {
            Core::FridaServerRequest request;
            request.Sdk = sdk;
            request.Serial = serial;
            request.Mode = mode;
            request.Cancel = &session.Cancel;
            return Core::StopFridaServer(request, localPort);
        });
    }

    void RefreshFridaProcesses(Context &context, const FridaDeviceSession &session) {
        context.Host.Frida.RefreshProcesses(session.Serial);
    }

    void AttachSelectedFridaProcess(Context &context, FridaDeviceSession &session) {
        if (session.SelectedProcess < 0 || std::cmp_greater_equal(session.SelectedProcess, session.Processes.size())) {
            return;
        }
        const FridaProcessRow &process = session.Processes.at(static_cast<std::size_t>(session.SelectedProcess));
        context.Host.Frida.Attach(session.Serial, process.Pid);
    }

    void SpawnFridaPackage(Context &context, FridaDeviceSession &session) {
        context.Host.Frida.Spawn(session.Serial, session.SpawnPackage);
    }

    void ResumeFridaSpawn(Context &context, const FridaDeviceSession &session) {
        context.Host.Frida.Resume(session.Serial, session.SuspendedPid);
    }

    void DetachFrida(Context &context, const FridaDeviceSession &session) {
        context.Host.Frida.Detach(session.Serial);
    }

    void RunFridaScript(Context &context, FridaDeviceSession &session) {
        const std::string project = ProjectRoot(context, session);
        if (project.empty()) {
            return;
        }
        context.Host.Frida.LoadUserScript(
            session.Serial,
            project,
            session.Script,
            context.Workspaces.Frida.ScriptMode == FridaScriptMode::Snippet,
            UseV8(context.Workspaces.Frida)
        );
    }

    void StopFridaScript(Context &context, const FridaDeviceSession &session) {
        context.Host.Frida.UnloadUserScript(session.Serial);
    }

    void RequestFridaModules(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "modules";
        PostAgent(context, session, std::move(call));
    }

    void RequestFridaExports(Context &context, FridaDeviceSession &session, const std::string &moduleName) {
        Core::FridaAgentCall call;
        call.Method = "exports";
        call.Module = moduleName;
        PostAgent(context, session, std::move(call));
    }

    void RequestFridaRead(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "read";
        call.Address = session.MemoryAddress;
        call.Size = static_cast<std::uint32_t>(session.MemorySize > 0 ? session.MemorySize : static_cast<int>(Core::FRIDA_PAGE_SIZE));
        session.PendingRead = PostAgent(context, session, std::move(call));
        try {
            session.MemoryBase = std::stoull(session.MemoryAddress, nullptr, 16);
        } catch (...) {
            session.MemoryBase = 0;
        }
    }

    void RequestFridaWrite(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "write";
        call.Address = session.MemoryAddress;
        session.PendingWrite = PostAgent(context, session, std::move(call), session.Memory);
    }

    void RequestFridaScan(Context &context, FridaDeviceSession &session) {
        std::string pattern;
        if (Core::FridaLooksLikeHex(session.MemoryFind)) {
            if (const auto normalized = Core::FridaNormalizeHex(session.MemoryFind)) {
                pattern = *normalized;
            }
        } else {
            pattern = Core::FridaHexPattern(session.MemoryFind, session.MemoryFindUtf16);
        }
        if (pattern.empty()) {
            SetTabStatus(session, FridaWorkspaceTab::Memory, Tone::Warning, Platform::Tr("Enter a hex pattern or a string."));
            return;
        }
        Core::FridaAgentCall call;
        call.Method = "scan";
        call.Pattern = std::move(pattern);
        if (session.SelectedModule >= 0 && std::cmp_less(session.SelectedModule, session.Modules.size())) {
            call.Module = session.Modules.at(static_cast<std::size_t>(session.SelectedModule)).Name;
        } else {
            call.Start = session.MemoryAddress;
            call.Size = Core::FRIDA_SCAN_LIMIT;
        }
        PostAgent(context, session, std::move(call));
    }

    void RequestJavaClasses(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "java.classes";
        call.Query = session.JavaQuery;
        PostAgent(context, session, std::move(call));
    }

    void RequestJavaMethods(Context &context, FridaDeviceSession &session, const std::string &className) {
        Core::FridaAgentCall call;
        call.Method = "java.methods";
        call.ClassName = className;
        PostAgent(context, session, std::move(call));
    }

    void RequestJavaHook(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "hook.java";
        call.ClassName = session.HookClass;
        call.Symbol = session.HookMethod;
        call.LogArgs = session.HookLogArgs;
        call.Backtrace = session.HookBacktrace;
        PostAgent(context, session, std::move(call));
    }

    void RequestNativeHook(Context &context, FridaDeviceSession &session) {
        Core::FridaAgentCall call;
        call.Method = "hook.native";
        call.Module = session.HookModule;
        call.Symbol = session.HookSymbol;
        call.Address = session.HookAddress;
        call.LogArgs = session.HookLogArgs;
        call.Backtrace = session.HookBacktrace;
        PostAgent(context, session, std::move(call));
    }

    void RequestUnhook(Context &context, FridaDeviceSession &session, const std::uint64_t hookId) {
        Core::FridaAgentCall call;
        call.Method = "unhook";
        call.HookId = hookId;
        std::erase_if(session.Hooks, [hookId](const FridaHookRow &hook) { return hook.Id == hookId; });
        PostAgent(context, session, std::move(call));
    }

    std::string FridaScriptsDirectory(const FridaState &workspace) {
        if (workspace.ScriptsPath[0] != '\0') {
            return workspace.ScriptsPath;
        }
        return App::FridaScriptsDirectory();
    }
}

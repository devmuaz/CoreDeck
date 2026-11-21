//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_host.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <future>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include <frida-core.h>

#include "core/frida/frida_compiler.h"
#include "core/frida/frida_messages.h"
#include "core/frida/frida_prelude.h"
#include "platform/sha256.h"

namespace CoreDeck::Core {
    struct ScriptSlot { // NOLINT(misc-use-internal-linkage)
        FridaScript *Script = nullptr;
        gulong Handler = 0;
    };

    struct DeviceSlot { // NOLINT(misc-use-internal-linkage)
        std::string Address;
        FridaDevice *Device = nullptr;
        FridaSession *Session = nullptr;
        gulong DetachHandler = 0;
        ScriptSlot Agent;
        ScriptSlot User;
        bool AgentV8 = false;
        std::string AgentHash;
        std::uint32_t Pid = 0;
    };

    struct SignalBinding { // NOLINT(misc-use-internal-linkage)
        FridaHostState *Host = nullptr;
        std::string Serial;
        bool Agent = false;
    };

    struct FridaHostState {
        std::mutex StartMutex;
        std::thread Thread;
        std::atomic<bool> Running{false};
        std::mutex QueueMutex;
        std::condition_variable QueueCv;
        std::deque<std::function<void()>> Jobs;
        bool Stop = false;
        FridaDeviceManager *Manager = nullptr;
        FridaCompiler *Compiler = nullptr;
        std::unordered_map<std::string, DeviceSlot> Sessions;
        std::mutex EventsMutex;
        std::vector<FridaEvent> Events;
        std::mutex ServersMutex;
        std::vector<FridaTrackedServer> Servers;
    };

    namespace {
        // Sync Frida calls run on this worker. Frida delivers signals on its own loop thread, and a *_sync call made there re-enters that loop.
        std::string TakeError(GError *&error) {
            std::string message = "Frida call failed.";
            if (error != nullptr) {
                if (error->message != nullptr) {
                    message = error->message;
                }
                g_error_free(error);
                error = nullptr;
            }
            return message;
        }

        void PushEvent(FridaHostState *state, FridaEvent event) {
            const std::scoped_lock lock(state->EventsMutex);
            state->Events.push_back(std::move(event));
        }

        void DestroyBinding(gpointer data, GClosure * /*closure*/) {
            delete static_cast<SignalBinding *>(data);
        }

        void OnDetached(
            FridaSession * /*session*/,
            const FridaSessionDetachReason /*reason*/,
            FridaCrash * /*crash*/,
            gpointer user
        ) {
            const auto *binding = static_cast<SignalBinding *>(user);
            if (binding == nullptr || binding->Host == nullptr) {
                return;
            }
            FridaEvent event;
            event.Serial = binding->Serial;
            event.Kind = FridaEventKind::Detached;
            event.Ok = false;
            event.Message = "Session detached.";
            PushEvent(binding->Host, std::move(event));
        }

        void OnScriptMessage(FridaScript * /*script*/, const gchar *message, GBytes *data, gpointer user) {
            const auto *binding = static_cast<SignalBinding *>(user);
            if (binding == nullptr || binding->Host == nullptr) {
                return;
            }
            FridaEvent event;
            event.Serial = binding->Serial;
            event.Agent = binding->Agent;
            event.Kind = FridaEventKind::Console;
            event.Ok = true;
            if (message != nullptr) {
                const FridaParsedMessage parsed = ParseFridaMessage(message);
                if (parsed.Rpc) {
                    event.Kind = FridaEventKind::Rpc;
                    event.Ok = parsed.Ok;
                    event.RequestId = parsed.Id;
                    event.Message = parsed.Error;
                    event.Json = parsed.ResultJson;
                } else if (parsed.IsHook) {
                    event.Kind = FridaEventKind::Hook;
                    event.RequestId = parsed.HookId;
                    event.Message = parsed.Console;
                } else {
                    event.Kind = parsed.ConsoleKind == FridaConsoleKind::Error ? FridaEventKind::Failed : FridaEventKind::Console;
                    event.Ok = parsed.ConsoleKind != FridaConsoleKind::Error;
                    event.Message = parsed.Console;
                }
            }
            if (data != nullptr) {
                gsize size = 0;
                const void *bytes = g_bytes_get_data(data, &size);
                if (bytes != nullptr && size > 0) {
                    const auto *begin = static_cast<const std::uint8_t *>(bytes);
                    event.Bytes.assign(begin, begin + size);
                }
            }
            PushEvent(binding->Host, std::move(event));
        }

        void ReleaseScript(ScriptSlot &slot) {
            if (slot.Script == nullptr) {
                return;
            }
            if (slot.Handler != 0) {
                g_signal_handler_disconnect(slot.Script, slot.Handler);
                slot.Handler = 0;
            }
            GError *error = nullptr;
            frida_script_unload_sync(slot.Script, nullptr, &error);
            if (error != nullptr) {
                g_error_free(error);
            }
            frida_unref(slot.Script);
            slot.Script = nullptr;
        }

        void ReleaseSession(const FridaHostState *state, DeviceSlot &slot) {
            ReleaseScript(slot.User);
            ReleaseScript(slot.Agent);
            if (slot.Session != nullptr) {
                if (slot.DetachHandler != 0) {
                    g_signal_handler_disconnect(slot.Session, slot.DetachHandler);
                    slot.DetachHandler = 0;
                }
                GError *error = nullptr;
                frida_session_detach_sync(slot.Session, nullptr, &error);
                if (error != nullptr) {
                    g_error_free(error);
                }
                frida_unref(slot.Session);
                slot.Session = nullptr;
            }
            if (slot.Device != nullptr && state->Manager != nullptr && !slot.Address.empty()) {
                GError *error = nullptr;
                frida_device_manager_remove_remote_device_sync(state->Manager, slot.Address.c_str(), nullptr, &error);
                if (error != nullptr) {
                    g_error_free(error);
                }
                frida_unref(slot.Device);
                slot.Device = nullptr;
            }
        }

        DeviceSlot *FindSlot(FridaHostState *state, const std::string &serial) {
            const auto found = state->Sessions.find(serial);
            if (found == state->Sessions.end()) {
                return nullptr;
            }
            return &found->second;
        }

        bool LoadScript(
            FridaHostState *state,
            const DeviceSlot &slot,
            ScriptSlot &scriptSlot,
            const std::string &serial,
            const std::string &name,
            const std::string &source,
            const bool useV8,
            const bool agent,
            std::string &error
        ) {
            if (slot.Session == nullptr) {
                error = "Attach to a process first.";
                return false;
            }
            ReleaseScript(scriptSlot);
            FridaScriptOptions *options = frida_script_options_new();
            frida_script_options_set_name(options, name.c_str());
            frida_script_options_set_runtime(options, useV8 ? FRIDA_SCRIPT_RUNTIME_V8 : FRIDA_SCRIPT_RUNTIME_QJS);
            GError *gerror = nullptr;
            FridaScript *script = frida_session_create_script_sync(slot.Session, source.c_str(), options, nullptr, &gerror);
            frida_unref(options);
            if (gerror != nullptr || script == nullptr) {
                error = TakeError(gerror);
                if (script != nullptr) {
                    frida_unref(script);
                }
                return false;
            }
            auto *binding = new SignalBinding{.Host = state, .Serial = serial, .Agent = agent};
            scriptSlot.Script = script;
            scriptSlot.Handler = g_signal_connect_data(
                script,
                "message",
                G_CALLBACK(OnScriptMessage),
                binding,
                DestroyBinding,
                static_cast<GConnectFlags>(0)
            );
            frida_script_load_sync(script, nullptr, &gerror);
            if (gerror != nullptr) {
                error = TakeError(gerror);
                ReleaseScript(scriptSlot);
                return false;
            }
            return true;
        }

        bool EnsureAgent(
            FridaHostState *state,
            DeviceSlot &slot,
            const std::string &serial,
            const std::string &projectRoot,
            const bool useV8,
            std::string &error
        ) {
            const std::filesystem::path source = std::filesystem::path(projectRoot) / "agent" / "coredeck_agent.js";
            const std::string hash = Platform::Sha256File(source.string());
            if (hash.empty()) {
                error = "The resident Frida agent is missing.";
                return false;
            }
            if (slot.Agent.Script != nullptr && slot.AgentHash == hash && slot.AgentV8 == useV8) {
                return true;
            }
            const FridaCompileResult compiled = CompileFridaBundle(
                reinterpret_cast<void **>(&state->Compiler),
                state->Manager,
                projectRoot,
                "agent/coredeck_agent.js"
            );
            if (!compiled.Ok) {
                error = compiled.Message;
                return false;
            }
            if (!LoadScript(state, slot, slot.Agent, serial, "coredeck-agent", compiled.Bundle, useV8, true, error)) {
                return false;
            }
            slot.AgentHash = hash;
            slot.AgentV8 = useV8;
            return true;
        }

        void ConnectDevice(FridaHostState *state, const std::string &serial, const std::string &address) {
            if (DeviceSlot *existing = FindSlot(state, serial)) {
                ReleaseSession(state, *existing);
                state->Sessions.erase(serial);
            }
            GError *error = nullptr;
            FridaDevice *device = frida_device_manager_add_remote_device_sync(
                state->Manager,
                address.c_str(),
                nullptr,
                nullptr,
                &error
            );
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Connected;
            if (error != nullptr || device == nullptr) {
                event.Ok = false;
                event.Message = TakeError(error);
                PushEvent(state, std::move(event));
                return;
            }
            GError *listError = nullptr;
            FridaProcessList *list = frida_device_enumerate_processes_sync(device, nullptr, nullptr, &listError);
            if (listError != nullptr || list == nullptr) {
                event.Ok = false;
                event.Message = TakeError(listError);
                GError *removeError = nullptr;
                frida_device_manager_remove_remote_device_sync(state->Manager, address.c_str(), nullptr, &removeError);
                if (removeError != nullptr) {
                    g_error_free(removeError);
                }
                frida_unref(device);
                PushEvent(state, std::move(event));
                return;
            }
            frida_unref(list);
            DeviceSlot slot;
            slot.Address = address;
            slot.Device = device;
            state->Sessions.insert_or_assign(serial, std::move(slot));
            event.Ok = true;
            event.Message = address;
            PushEvent(state, std::move(event));
        }

        void CollectProcesses(FridaHostState *state, const std::string &serial) {
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Processes;
            const DeviceSlot *slot = FindSlot(state, serial);
            if (slot == nullptr || slot->Device == nullptr) {
                event.Message = "Connect Frida before listing processes.";
                PushEvent(state, std::move(event));
                return;
            }
            GError *error = nullptr;
            FridaProcessList *list = frida_device_enumerate_processes_sync(slot->Device, nullptr, nullptr, &error);
            if (error != nullptr || list == nullptr) {
                event.Message = TakeError(error);
                PushEvent(state, std::move(event));
                return;
            }
            const gint count = frida_process_list_size(list);
            event.Processes.reserve(static_cast<std::size_t>(count));
            for (gint index = 0; index < count; ++index) {
                FridaProcess *process = frida_process_list_get(list, index);
                if (process == nullptr) {
                    continue;
                }
                FridaProcessInfo info;
                info.Pid = frida_process_get_pid(process);
                const gchar *name = frida_process_get_name(process);
                info.Name = name != nullptr ? name : "";
                event.Processes.push_back(std::move(info));
                g_object_unref(process);
            }
            frida_unref(list);
            event.Ok = true;
            PushEvent(state, std::move(event));
        }

        bool AttachPid(FridaHostState *state, DeviceSlot &slot, const std::string &serial, const std::uint32_t pid, std::string &error) {
            if (slot.Session != nullptr) {
                ReleaseScript(slot.User);
                ReleaseScript(slot.Agent);
                if (slot.DetachHandler != 0) {
                    g_signal_handler_disconnect(slot.Session, slot.DetachHandler);
                    slot.DetachHandler = 0;
                }
                frida_unref(slot.Session);
                slot.Session = nullptr;
            }
            GError *gerror = nullptr;
            FridaSession *session = frida_device_attach_sync(slot.Device, pid, nullptr, nullptr, &gerror);
            if (gerror != nullptr || session == nullptr) {
                error = TakeError(gerror);
                return false;
            }
            auto *binding = new SignalBinding{.Host = state, .Serial = serial, .Agent = false};
            slot.Session = session;
            slot.Pid = pid;
            slot.DetachHandler = g_signal_connect_data(
                session,
                "detached",
                G_CALLBACK(OnDetached),
                binding,
                DestroyBinding,
                static_cast<GConnectFlags>(0)
            );
            return true;
        }

        void AttachProcess(FridaHostState *state, const std::string &serial, const std::uint32_t pid) {
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Attached;
            event.Pid = pid;
            DeviceSlot *slot = FindSlot(state, serial);
            if (slot == nullptr || slot->Device == nullptr) {
                event.Message = "Connect Frida before attaching.";
                PushEvent(state, std::move(event));
                return;
            }
            std::string error;
            if (!AttachPid(state, *slot, serial, pid, error)) {
                event.Message = std::move(error);
                PushEvent(state, std::move(event));
                return;
            }
            event.Ok = true;
            PushEvent(state, std::move(event));
        }

        void SpawnProcess(FridaHostState *state, const std::string &serial, const std::string &identifier) {
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Spawned;
            DeviceSlot *slot = FindSlot(state, serial);
            if (slot == nullptr || slot->Device == nullptr || identifier.empty()) {
                event.Message = identifier.empty() ? "Enter a package name." : "Connect Frida before spawning.";
                PushEvent(state, std::move(event));
                return;
            }
            GError *error = nullptr;
            const guint pid = frida_device_spawn_sync(slot->Device, identifier.c_str(), nullptr, nullptr, &error);
            if (error != nullptr || pid == 0) {
                event.Message = TakeError(error);
                PushEvent(state, std::move(event));
                return;
            }
            event.Pid = pid;
            std::string attachError;
            if (!AttachPid(state, *slot, serial, pid, attachError)) {
                event.Message = std::move(attachError);
                PushEvent(state, std::move(event));
                return;
            }
            event.Ok = true;
            event.Message = identifier;
            PushEvent(state, std::move(event));
        }

        void ResumeProcess(FridaHostState *state, const std::string &serial, const std::uint32_t pid) {
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Resumed;
            event.Pid = pid;
            const DeviceSlot *slot = FindSlot(state, serial);
            if (slot == nullptr || slot->Device == nullptr || pid == 0) {
                event.Message = "Nothing is waiting to resume.";
                PushEvent(state, std::move(event));
                return;
            }
            GError *error = nullptr;
            frida_device_resume_sync(slot->Device, pid, nullptr, &error);
            if (error != nullptr) {
                event.Message = TakeError(error);
                PushEvent(state, std::move(event));
                return;
            }
            event.Ok = true;
            PushEvent(state, std::move(event));
        }

        void ThreadMain(FridaHostState *state) {
            frida_init();
            state->Manager = frida_device_manager_new();
            for (;;) {
                std::function<void()> job;
                {
                    std::unique_lock lock(state->QueueMutex);
                    state->QueueCv.wait(lock, [&state] { return state->Stop || !state->Jobs.empty(); });
                    if (state->Stop && state->Jobs.empty()) {
                        break;
                    }
                    job = std::move(state->Jobs.front());
                    state->Jobs.pop_front();
                }
                job();
            }
            for (auto &entry: state->Sessions) {
                ReleaseSession(state, entry.second);
            }
            state->Sessions.clear();
            if (state->Compiler != nullptr) {
                frida_unref(state->Compiler);
                state->Compiler = nullptr;
            }
            if (state->Manager != nullptr) {
                frida_unref(state->Manager);
                state->Manager = nullptr;
            }
        }
    }

    FridaHost::FridaHost() : m_State(std::make_unique<FridaHostState>()) {
    }

    FridaHost::~FridaHost() {
        std::vector<FridaTrackedServer> servers;
        {
            const std::scoped_lock lock(m_State->ServersMutex);
            servers.swap(m_State->Servers);
        }
        Shutdown();
        for (const FridaTrackedServer &server: servers) {
            FridaServerRequest request;
            request.Sdk = server.Sdk;
            request.Serial = server.Serial;
            request.Mode = server.Mode;
            StopFridaServer(request, server.LocalPort);
        }
    }

    void FridaHost::m_Post(std::function<void()> job) {
        Start();
        {
            const std::scoped_lock lock(m_State->QueueMutex);
            if (m_State->Stop) {
                return;
            }
            m_State->Jobs.push_back(std::move(job));
        }
        m_State->QueueCv.notify_one();
    }

    void FridaHost::Start() {
        const std::scoped_lock lock(m_State->StartMutex);
        if (m_State->Thread.joinable()) {
            return;
        }
        m_State->Stop = false;
        m_State->Running.store(true);
        m_State->Thread = std::thread([state = m_State.get()] { ThreadMain(state); });
    }

    void FridaHost::Shutdown() {
        std::thread worker;
        {
            const std::scoped_lock lock(m_State->StartMutex);
            {
                const std::scoped_lock queue(m_State->QueueMutex);
                m_State->Stop = true;
            }
            m_State->QueueCv.notify_one();
            if (m_State->Thread.joinable()) {
                worker = std::move(m_State->Thread);
            }
        }
        if (!worker.joinable()) {
            m_State->Running.store(false);
            return;
        }
        std::atomic<bool> finished{false};
        std::thread joiner([&worker, &finished] {
            worker.join();
            finished.store(true);
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!finished.load()) {
            joiner.detach();
            std::_Exit(0);
        }
        joiner.join();
        m_State->Running.store(false);
    }

    bool FridaHost::Probe(std::string &error) {
        const auto promise = std::make_shared<std::promise<std::pair<bool, std::string>>>();
        auto future = promise->get_future();
        m_Post([state = m_State.get(), promise] {
            GError *gerror = nullptr;
            FridaDeviceList *devices = frida_device_manager_enumerate_devices_sync(state->Manager, nullptr, &gerror);
            std::string message;
            const bool ok = gerror == nullptr && devices != nullptr && frida_device_list_size(devices) > 0;
            if (!ok) {
                message = gerror != nullptr ? TakeError(gerror) : "Frida did not report a local device.";
            } else if (gerror != nullptr) {
                g_error_free(gerror);
            }
            if (devices != nullptr) {
                frida_unref(devices);
            }
            promise->set_value({ok, std::move(message)});
        });
        if (future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
            error = "Frida did not answer.";
            return false;
        }
        const auto result = future.get();
        error = result.second;
        return result.first;
    }

    void FridaHost::NoteServer(const FridaTrackedServer &server) {
        const std::scoped_lock lock(m_State->ServersMutex);
        std::erase_if(m_State->Servers, [&server](const FridaTrackedServer &item) { return item.Serial == server.Serial; });
        m_State->Servers.push_back(server);
    }

    void FridaHost::ForgetServer(const std::string &serial) {
        const std::scoped_lock lock(m_State->ServersMutex);
        std::erase_if(m_State->Servers, [&serial](const FridaTrackedServer &item) { return item.Serial == serial; });
    }

    void FridaHost::Connect(const std::string &serial, const std::string &address) {
        m_Post([state = m_State.get(), serial, address] { ConnectDevice(state, serial, address); });
    }

    void FridaHost::Disconnect(const std::string &serial) {
        m_Post([state = m_State.get(), serial] {
            if (DeviceSlot *slot = FindSlot(state, serial)) {
                ReleaseSession(state, *slot);
                state->Sessions.erase(serial);
            }
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Disconnected;
            event.Ok = true;
            PushEvent(state, std::move(event));
        });
    }

    void FridaHost::RefreshProcesses(const std::string &serial) {
        m_Post([state = m_State.get(), serial] { CollectProcesses(state, serial); });
    }

    void FridaHost::Attach(const std::string &serial, const std::uint32_t pid) {
        m_Post([state = m_State.get(), serial, pid] { AttachProcess(state, serial, pid); });
    }

    void FridaHost::Spawn(const std::string &serial, const std::string &identifier) {
        m_Post([state = m_State.get(), serial, identifier] { SpawnProcess(state, serial, identifier); });
    }

    void FridaHost::Resume(const std::string &serial, const std::uint32_t pid) {
        m_Post([state = m_State.get(), serial, pid] { ResumeProcess(state, serial, pid); });
    }

    void FridaHost::Detach(const std::string &serial) {
        m_Post([state = m_State.get(), serial] {
            if (DeviceSlot *slot = FindSlot(state, serial)) {
                ReleaseScript(slot->User);
                ReleaseScript(slot->Agent);
                if (slot->Session != nullptr) {
                    if (slot->DetachHandler != 0) {
                        g_signal_handler_disconnect(slot->Session, slot->DetachHandler);
                        slot->DetachHandler = 0;
                    }
                    GError *error = nullptr;
                    frida_session_detach_sync(slot->Session, nullptr, &error);
                    if (error != nullptr) {
                        g_error_free(error);
                    }
                    frida_unref(slot->Session);
                    slot->Session = nullptr;
                }
                slot->Pid = 0;
            }
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Detached;
            event.Ok = true;
            PushEvent(state, std::move(event));
        });
    }

    void FridaHost::LoadUserScript(
        const std::string &serial,
        const std::string &projectRoot,
        const std::string &source,
        const bool snippet,
        const bool useV8
    ) {
        m_Post([state = m_State.get(), serial, projectRoot, source, snippet, useV8] {
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Script;
            event.Agent = false;
            DeviceSlot *slot = FindSlot(state, serial);
            if (slot == nullptr || slot->Session == nullptr) {
                event.Message = "Attach to a process before running a script.";
                PushEvent(state, std::move(event));
                return;
            }
            const std::filesystem::path directory = std::filesystem::path(projectRoot) / ".coredeck";
            std::error_code failure;
            std::filesystem::create_directories(directory, failure);
            const char *name = snippet ? "snippet.js" : "module.js";
            const std::filesystem::path file = directory / name;
            {
                std::ofstream output(file, std::ios::trunc);
                output << (snippet ? WrapFridaSnippet(source) : source);
            }
            const FridaCompileResult compiled = CompileFridaBundle(
                reinterpret_cast<void **>(&state->Compiler),
                state->Manager,
                projectRoot,
                std::string(".coredeck/") + name
            );
            if (!compiled.Ok) {
                event.Message = compiled.Message;
                PushEvent(state, std::move(event));
                return;
            }
            std::string error;
            if (!LoadScript(state, *slot, slot->User, serial, "coredeck-user", compiled.Bundle, useV8, false, error)) {
                event.Message = std::move(error);
                PushEvent(state, std::move(event));
                return;
            }
            event.Ok = true;
            event.Message = "Script loaded.";
            PushEvent(state, std::move(event));
        });
    }

    void FridaHost::UnloadUserScript(const std::string &serial) {
        m_Post([state = m_State.get(), serial] {
            if (DeviceSlot *slot = FindSlot(state, serial)) {
                ReleaseScript(slot->User);
            }
            FridaEvent event;
            event.Serial = serial;
            event.Kind = FridaEventKind::Script;
            event.Ok = true;
            event.Message = "Script stopped.";
            PushEvent(state, std::move(event));
        });
    }

    void FridaHost::PostAgent(
        const std::string &serial,
        const std::string &projectRoot,
        const bool useV8,
        const std::uint64_t requestId,
        const std::string &json,
        const std::vector<std::uint8_t> &bytes
    ) {
        m_Post([state = m_State.get(), serial, projectRoot, useV8, requestId, json, bytes] {
            DeviceSlot *slot = FindSlot(state, serial);
            FridaEvent failed;
            failed.Serial = serial;
            failed.Kind = FridaEventKind::Rpc;
            failed.RequestId = requestId;
            if (slot == nullptr || slot->Session == nullptr) {
                failed.Message = "Attach to a process first.";
                PushEvent(state, std::move(failed));
                return;
            }
            std::string error;
            if (!EnsureAgent(state, *slot, serial, projectRoot, useV8, error)) {
                failed.Message = std::move(error);
                PushEvent(state, std::move(failed));
                return;
            }
            GBytes *payload = nullptr;
            if (!bytes.empty()) {
                payload = g_bytes_new(bytes.data(), bytes.size());
            }
            frida_script_post(slot->Agent.Script, json.c_str(), payload);
            if (payload != nullptr) {
                g_bytes_unref(payload);
            }
        });
    }

    std::vector<FridaEvent> FridaHost::Drain() {
        const std::scoped_lock lock(m_State->EventsMutex);
        std::vector<FridaEvent> events;
        events.swap(m_State->Events);
        return events;
    }
}

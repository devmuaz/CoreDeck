//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_HOST_H
#define COREDECK_CORE_FRIDA_FRIDA_HOST_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/frida/frida_server.h"

namespace CoreDeck::Core {
    enum class FridaEventKind : uint8_t {
        Connected,
        Disconnected,
        Processes,
        Attached,
        Spawned,
        Resumed,
        Detached,
        Script,
        Console,
        Hook,
        Rpc,
        Failed,
    };

    struct FridaProcessInfo {
        std::uint32_t Pid = 0;
        std::string Name;
    };

    struct FridaTrackedServer {
        SdkInfo Sdk;
        std::string Serial;
        FridaServerMode Mode = FridaServerMode::Managed;
        int LocalPort = 0;
    };

    struct FridaEvent {
        std::string Serial;
        FridaEventKind Kind = FridaEventKind::Failed;
        bool Ok = false;
        bool Agent = false;
        std::string Message;
        std::string Json;
        std::uint32_t Pid = 0;
        std::uint64_t RequestId = 0;
        std::vector<std::uint8_t> Bytes;
        std::vector<FridaProcessInfo> Processes;
    };

    struct FridaHostState;

    class FridaHost {
    public:
        FridaHost();
        FridaHost(const FridaHost &) = delete;
        FridaHost &operator=(const FridaHost &) = delete;
        FridaHost(FridaHost &&) = delete;
        FridaHost &operator=(FridaHost &&) = delete;
        ~FridaHost();

        void Start();

        void Shutdown();

        bool Probe(std::string &error);

        void NoteServer(const FridaTrackedServer &server);

        void ForgetServer(const std::string &serial);

        void Connect(const std::string &serial, const std::string &address);

        void Disconnect(const std::string &serial);

        void RefreshProcesses(const std::string &serial);

        void Attach(const std::string &serial, std::uint32_t pid);

        void Spawn(const std::string &serial, const std::string &identifier);

        void Resume(const std::string &serial, std::uint32_t pid);

        void Detach(const std::string &serial);

        void LoadUserScript(
            const std::string &serial,
            const std::string &projectRoot,
            const std::string &source,
            bool snippet,
            bool useV8
        );

        void UnloadUserScript(const std::string &serial);

        void PostAgent(
            const std::string &serial,
            const std::string &projectRoot,
            bool useV8,
            std::uint64_t requestId,
            const std::string &json,
            const std::vector<std::uint8_t> &bytes
        );

        std::vector<FridaEvent> Drain();

    private:
        void m_Post(std::function<void()> job);

        std::unique_ptr<FridaHostState> m_State;
    };
}

#endif // COREDECK_CORE_FRIDA_FRIDA_HOST_H

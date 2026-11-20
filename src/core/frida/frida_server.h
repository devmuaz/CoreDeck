//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_SERVER_H
#define COREDECK_CORE_FRIDA_FRIDA_SERVER_H

#include <atomic>
#include <cstdint>
#include <string>

#include "core/sdk/sdk.h"

namespace CoreDeck::Core {
    enum class FridaStatusCode : uint8_t {
        None,
        InvalidRequest,
        ToolMissing,
        DownloadFailed,
        HashMismatch,
        DecompressFailed,
        CommandFailed,
        NotRoot,
        Cancelled,
    };

    struct FridaStatus {
        FridaStatusCode Code = FridaStatusCode::None;
        std::string Message;

        bool Ok() const {
            return Code == FridaStatusCode::None;
        }
    };

    enum class FridaServerMode : uint8_t {
        Managed,
        Existing,
    };

    struct FridaEndpoint {
        FridaStatus Status;
        std::string Abi;
        std::string Asset;
        bool Root = false;
        bool Managed = false;
        int DevicePort = 0;
        int LocalPort = 0;
        std::string Address;

        bool Ok() const {
            return Status.Ok() && LocalPort > 0;
        }
    };

    struct FridaServerRequest {
        SdkInfo Sdk;
        std::string Serial;
        std::string CacheDirectory;
        FridaServerMode Mode = FridaServerMode::Managed;
        int DevicePort = 0;
        const std::atomic<bool> *Cancel = nullptr;
        std::atomic<std::uint64_t> *Received = nullptr;
        std::atomic<std::uint64_t> *Total = nullptr;
        std::atomic<const char *> *AssetLabel = nullptr;
    };

    FridaEndpoint StartFridaServer(const FridaServerRequest &request);

    FridaStatus StopFridaServer(const FridaServerRequest &request, int localPort);
}

#endif // COREDECK_CORE_FRIDA_FRIDA_SERVER_H

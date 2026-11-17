//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_server.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <vector>

#include "core/adb/adb_client.h"
#include "core/frida/frida_abi.h"
#include "platform/build_info.h"
#include "platform/http_download.h"
#include "platform/sha256.h"
#include "platform/strings.h"

#define XZ_USE_CRC64
#include "xz.h"

namespace CoreDeck::Core {
    namespace {
        bool Cancelled(const FridaServerRequest &request) {
            return request.Cancel != nullptr && request.Cancel->load();
        }

        FridaEndpoint Failed(const FridaStatusCode code, std::string message) {
            FridaEndpoint endpoint;
            endpoint.Status.Code = code;
            endpoint.Status.Message = std::move(message);
            return endpoint;
        }

        std::string CommandText(const AdbCommandResult &result) {
            std::string text = result.Output();
            if (!text.empty()) {
                return text;
            }
            if (result.Error == AdbError::ToolMissing) {
                return "adb was not found.";
            }
            if (result.Error == AdbError::Cancelled) {
                return "Cancelled.";
            }
            return "adb command failed.";
        }

        bool EnsureParent(const std::filesystem::path &path, std::string &error) {
            std::error_code failure;
            std::filesystem::create_directories(path.parent_path(), failure);
            if (failure) {
                error = failure.message();
                return false;
            }
            return true;
        }
    }

    bool DecompressXz(const std::string &sourcePath, const std::string &destPath, std::string &error) {
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input) {
            error = "Could not read the xz archive.";
            return false;
        }
        const std::vector<std::uint8_t> compressed{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>(),
        };
        if (compressed.empty()) {
            error = "The xz archive is empty.";
            return false;
        }
        if (!EnsureParent(destPath, error)) {
            return false;
        }
        xz_crc32_init();
        xz_crc64_init();
        struct xz_dec *decoder = xz_dec_init(XZ_DYNALLOC, static_cast<uint32_t>(1U << 26U));
        if (decoder == nullptr) {
            error = "Could not start the xz decoder.";
            return false;
        }
        std::ofstream output(destPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            xz_dec_end(decoder);
            error = "Could not write the decompressed server.";
            return false;
        }
        std::vector<std::uint8_t> chunk(static_cast<std::size_t>(1U << 20U));
        xz_buf stream{};
        stream.in = compressed.data();
        stream.in_size = compressed.size();
        xz_ret result = XZ_OK;
        while (result == XZ_OK) {
            const std::size_t inPos = stream.in_pos;
            stream.out = chunk.data();
            stream.out_pos = 0;
            stream.out_size = chunk.size();
            result = xz_dec_run(decoder, &stream);
            if (stream.out_pos > 0) {
                output.write(reinterpret_cast<const char *>(chunk.data()), static_cast<std::streamsize>(stream.out_pos));
            }
            if (result == XZ_OK && stream.in_pos == inPos && stream.out_pos == 0) {
                result = XZ_DATA_ERROR;
                break;
            }
        }
        xz_dec_end(decoder);
        output.close();
        if (result != XZ_STREAM_END || !output) {
            std::error_code failure;
            std::filesystem::remove(destPath, failure);
            error = "The xz archive could not be decompressed.";
            return false;
        }
        return true;
    }

    namespace {
        std::optional<FridaEndpoint> CacheServerBinary(
            const FridaServerRequest &request,
            const std::filesystem::path &binary,
            const std::string &asset
        ) {
            std::error_code failure;
            if (std::filesystem::is_regular_file(binary, failure)) {
                return std::nullopt;
            }
            const std::filesystem::path archive = binary.string() + ".xz";
            std::string parentError;
            if (!EnsureParent(archive, parentError)) {
                return Failed(FridaStatusCode::DownloadFailed, parentError);
            }
            const char *expected = FridaServerSha256(asset);
            if (expected == nullptr) {
                return Failed(FridaStatusCode::HashMismatch, "No checksum is pinned for this server.");
            }
            std::string downloadError;
            const bool downloaded = Platform::HttpDownloadToFile(
                FridaServerUrl(asset),
                archive.string(),
                Platform::StrConcat(Platform::BuildInfo::TITLE, "/", Platform::BuildInfo::VERSION),
                [&request](const std::uint64_t received, const std::uint64_t total) {
                    if (request.Received != nullptr) {
                        request.Received->store(received);
                    }
                    if (request.Total != nullptr) {
                        request.Total->store(total);
                    }
                    return !Cancelled(request);
                },
                downloadError
            );
            if (!downloaded) {
                std::filesystem::remove(archive, failure);
                if (downloadError == "Cancelled." || Cancelled(request)) {
                    return Failed(FridaStatusCode::Cancelled, "Cancelled.");
                }
                return Failed(FridaStatusCode::DownloadFailed, downloadError.empty() ? "Download failed." : downloadError);
            }
            if (!Platform::EqualsIgnoreCaseHex(Platform::Sha256File(archive.string()), expected)) {
                std::filesystem::remove(archive, failure);
                return Failed(FridaStatusCode::HashMismatch, "The downloaded frida-server checksum did not match.");
            }
            std::string decompressError;
            if (!DecompressXz(archive.string(), binary.string(), decompressError)) {
                std::filesystem::remove(archive, failure);
                return Failed(FridaStatusCode::DecompressFailed, decompressError);
            }
            std::filesystem::remove(archive, failure);
            return std::nullopt;
        }

        bool ProcessRunning(const FridaServerRequest &request, const char *name) {
            const AdbCommandResult probe = RunAdbShell(request.Sdk, request.Serial, "ps -A", request.Cancel);
            return probe.Error != AdbError::Cancelled && probe.Stdout.find(name) != std::string::npos;
        }

        AdbCommandResult RootedShellId(const FridaServerRequest &request) {
            RunAdb(request.Sdk, {"root"}, request.Serial, request.Cancel);
            AdbCommandResult id;
            for (int attempt = 0; attempt < 8; ++attempt) {
                if (Cancelled(request)) {
                    id.Error = AdbError::Cancelled;
                    return id;
                }
                id = RunAdbShell(request.Sdk, request.Serial, "id", request.Cancel);
                if (id.Ok() || id.Error == AdbError::Cancelled) {
                    return id;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
            return id;
        }

        std::optional<FridaEndpoint> StartManagedServer(const FridaServerRequest &request, FridaEndpoint &endpoint) {
            if (request.CacheDirectory.empty()) {
                return Failed(FridaStatusCode::InvalidRequest, "The Frida cache directory is unavailable.");
            }
            const std::filesystem::path binary = std::filesystem::path(request.CacheDirectory) / endpoint.Asset;
            if (const std::optional<FridaEndpoint> cached = CacheServerBinary(request, binary, endpoint.Asset)) {
                return cached;
            }
            if (Cancelled(request)) {
                return Failed(FridaStatusCode::Cancelled, "Cancelled.");
            }

            const AdbCommandResult pushed = PushAdbFile(
                request.Sdk,
                request.Serial,
                binary.string(),
                FRIDA_SERVER_REMOTE_PATH,
                request.Cancel
            );
            if (!pushed.Ok()) {
                return Failed(
                    pushed.Error == AdbError::Cancelled ? FridaStatusCode::Cancelled : FridaStatusCode::CommandFailed,
                    CommandText(pushed)
                );
            }
            const AdbCommandResult chmod = RunAdbShell(
                request.Sdk,
                request.Serial,
                Platform::StrConcat("chmod 755 ", FRIDA_SERVER_REMOTE_PATH),
                request.Cancel
            );
            if (!chmod.Ok()) {
                return Failed(
                    chmod.Error == AdbError::Cancelled ? FridaStatusCode::Cancelled : FridaStatusCode::CommandFailed,
                    CommandText(chmod)
                );
            }

            const AdbCommandResult idResult = RootedShellId(request);
            if (idResult.Error == AdbError::Cancelled) {
                return Failed(FridaStatusCode::Cancelled, "Cancelled.");
            }
            endpoint.Root = ShellIsRoot(idResult.Stdout);
            bool useSu = false;
            if (!endpoint.Root) {
                const AdbCommandResult suId = RunAdbShell(request.Sdk, request.Serial, "su -c id", request.Cancel);
                if (suId.Error == AdbError::Cancelled) {
                    return Failed(FridaStatusCode::Cancelled, "Cancelled.");
                }
                useSu = ShellIsRoot(suId.Stdout);
                endpoint.Root = useSu;
            }
            if (!endpoint.Root) {
                return Failed(
                    FridaStatusCode::NotRoot,
                    "frida-server needs root. Use an emulator image that allows adb root, or attach to a server that is already running."
                );
            }

            const AdbCommandResult started = RunAdbShell(
                request.Sdk,
                request.Serial,
                FridaDaemonCommand(useSu),
                request.Cancel
            );
            if (started.Error == AdbError::Cancelled || Cancelled(request)) {
                return Failed(FridaStatusCode::Cancelled, "Cancelled.");
            }
            if (!ProcessRunning(request, "coredeck-frida")) {
                const std::string detail = CommandText(started);
                return Failed(
                    FridaStatusCode::CommandFailed,
                    detail == "adb command failed." ? "frida-server did not stay running." : detail
                );
            }
            return std::nullopt;
        }

        std::optional<FridaEndpoint> LaunchOrResume(const FridaServerRequest &request, FridaEndpoint &endpoint) {
            const AdbCommandResult ps = RunAdbShell(request.Sdk, request.Serial, "ps -A", request.Cancel);
            if (ps.Error == AdbError::Cancelled || Cancelled(request)) {
                return Failed(FridaStatusCode::Cancelled, "Cancelled.");
            }
            const FridaServerPresence found = FridaServersInProcessList(ps.Stdout);
            if (request.Mode == FridaServerMode::Managed) {
                if (found.CoreDeck) {
                    return std::nullopt;
                }
                return StartManagedServer(request, endpoint);
            }
            if (found.Foreign) {
                const AdbCommandResult idResult = RunAdbShell(request.Sdk, request.Serial, "id", request.Cancel);
                if (idResult.Error == AdbError::Cancelled) {
                    return Failed(FridaStatusCode::Cancelled, "Cancelled.");
                }
                endpoint.Root = ShellIsRoot(idResult.Stdout);
                return std::nullopt;
            }
            if (found.CoreDeck) {
                endpoint.Managed = true;
                endpoint.DevicePort = FRIDA_MANAGED_PORT;
                return std::nullopt;
            }
            return Failed(FridaStatusCode::CommandFailed, "No frida-server is running on this device.");
        }
    }

    FridaEndpoint StartFridaServer(const FridaServerRequest &request) {
        if (request.Serial.empty() || request.Sdk.AdbPath.empty()) {
            return Failed(
                request.Sdk.AdbPath.empty() ? FridaStatusCode::ToolMissing : FridaStatusCode::InvalidRequest,
                request.Sdk.AdbPath.empty() ? "adb was not found." : "Select a device first."
            );
        }
        if (Cancelled(request)) {
            return Failed(FridaStatusCode::Cancelled, "Cancelled.");
        }

        const AdbCommandResult abiResult = RunAdbShell(request.Sdk, request.Serial, "getprop ro.product.cpu.abi", request.Cancel);
        if (abiResult.Error == AdbError::Cancelled) {
            return Failed(FridaStatusCode::Cancelled, "Cancelled.");
        }
        std::string_view resolved = FridaAndroidAsset(abiResult.Stdout);
        if (resolved.empty()) {
            const AdbCommandResult listResult = RunAdbShell(
                request.Sdk,
                request.Serial,
                "getprop ro.product.cpu.abilist",
                request.Cancel
            );
            resolved = FirstFridaAndroidAsset(listResult.Stdout);
        }
        if (resolved.empty()) {
            return Failed(FridaStatusCode::CommandFailed, "This device ABI is not supported by frida-server.");
        }
        if (request.AssetLabel != nullptr) {
            request.AssetLabel->store(resolved.data());
        }
        const std::string asset(resolved);

        FridaEndpoint endpoint;
        endpoint.Asset = asset;
        endpoint.Abi = Platform::Trim(abiResult.Stdout);
        endpoint.Managed = request.Mode == FridaServerMode::Managed;
        if (request.Mode == FridaServerMode::Managed) {
            endpoint.DevicePort = FRIDA_MANAGED_PORT;
        } else if (request.DevicePort > 0) {
            endpoint.DevicePort = request.DevicePort;
        } else {
            endpoint.DevicePort = FRIDA_EXISTING_PORT;
        }

        if (const std::optional<FridaEndpoint> launched = LaunchOrResume(request, endpoint)) {
            return *launched;
        }

        const AdbCommandResult forwarded = RunAdb(
            request.Sdk,
            {"forward", "tcp:0", Platform::StrConcat("tcp:", std::to_string(endpoint.DevicePort))},
            request.Serial,
            request.Cancel
        );
        if (!forwarded.Ok()) {
            return Failed(FridaStatusCode::CommandFailed, CommandText(forwarded));
        }
        const std::optional<int> port = ParseForwardedPort(forwarded.Stdout);
        if (!port) {
            return Failed(FridaStatusCode::CommandFailed, "adb did not report a local forward port.");
        }
        endpoint.LocalPort = *port;
        endpoint.Address = Platform::StrConcat("127.0.0.1:", std::to_string(*port));
        endpoint.Status.Message = endpoint.Managed ? "Frida server is running." : "Attached to the existing Frida server.";
        return endpoint;
    }

    FridaStatus StopFridaServer(const FridaServerRequest &request, const int localPort) {
        FridaStatus status;
        if (request.Serial.empty() || request.Sdk.AdbPath.empty()) {
            status.Code = FridaStatusCode::InvalidRequest;
            status.Message = "Select a device first.";
            return status;
        }
        if (localPort > 0) {
            RunAdb(
                request.Sdk,
                {"forward", "--remove", Platform::StrConcat("tcp:", std::to_string(localPort))},
                request.Serial,
                request.Cancel
            );
        }
        if (request.Mode == FridaServerMode::Managed) {
            const AdbCommandResult idResult = RunAdbShell(request.Sdk, request.Serial, "id", request.Cancel);
            const bool rootShell = ShellIsRoot(idResult.Stdout);
            const AdbCommandResult killed = RunAdbShell(
                request.Sdk,
                request.Serial,
                FridaKillCommand(!rootShell),
                request.Cancel
            );
            if (!killed.Ok() && killed.ExitCode != 1) {
                status.Code = FridaStatusCode::CommandFailed;
                status.Message = CommandText(killed);
                return status;
            }
        }
        status.Message = "Frida server stopped.";
        return status;
    }
}

//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_CLIENT_H
#define COREDECK_ADB_CLIENT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/sdk.h"

namespace CoreDeck::Core {
    enum class AdbError : uint8_t {
        None,
        ToolMissing,
        InvalidRequest,
        StartFailed,
        CommandFailed,
        Cancelled,
    };

    struct AdbCommandResult {
        AdbError Error = AdbError::None;
        bool Started = false;
        int ExitCode = -1;
        std::string Stdout;
        std::string Stderr;

        bool Ok() const {
            return Error == AdbError::None && Started && ExitCode == 0;
        }

        std::string Output() const {
            std::string text = Stdout;
            if (!Stderr.empty()) {
                if (!text.empty() && text.back() != '\n') {
                    text.push_back('\n');
                }
                text += Stderr;
            }
            return text;
        }
    };

    struct AdbInstallOptions {
        bool Replace = true;
        bool AllowDowngrade = false;
        bool GrantRuntimePermissions = false;
    };

    std::vector<std::string> BuildAdbArguments(
        const std::string &serial,
        const std::vector<std::string> &command
    );

    std::string ParseAdbVersion(const std::string &output);

    bool IsValidAdbNetworkEndpoint(const std::string &endpoint);

    bool IsValidAdbSocketSpec(const std::string &endpoint);

    bool AdbConnectReportSucceeded(const std::string &output);

    bool AdbDisconnectReportSucceeded(const std::string &output);

    std::optional<std::string> ParseWirelessIpv4(const std::string &text);

    std::optional<std::string> FindAdbNetworkEndpoint(const std::string &text);

    AdbCommandResult ConnectAdbDeviceOverWifi(
        const SdkInfo &sdk,
        const std::string &serial,
        int port,
        const std::atomic<bool> *cancel = nullptr
    );

    AdbCommandResult RunAdb(
        const SdkInfo &sdk,
        const std::vector<std::string> &command,
        const std::string &serial = {},
        const std::atomic<bool> *cancel = nullptr,
        std::size_t outputLimit = static_cast<std::size_t>(16U * 1024U * 1024U)
    );

    AdbCommandResult RunAdbShell(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &command,
        const std::atomic<bool> *cancel = nullptr
    );

    AdbCommandResult InstallAdbPackage(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &apkPath,
        const AdbInstallOptions &options,
        const std::atomic<bool> *cancel = nullptr
    );

    std::vector<std::string> UninstallAdbCommand(const std::string &packageName, bool keepData);

    enum class AdbPackageListScope : uint8_t {
        All,
        ThirdParty,
        System,
    };

    std::vector<std::string> ListAdbPackagesCommand(AdbPackageListScope scope = AdbPackageListScope::All);

    std::vector<std::string> ParseAdbPackages(const std::string &output);

    AdbCommandResult UninstallAdbPackage(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &packageName,
        bool keepData,
        const std::atomic<bool> *cancel = nullptr
    );

    AdbCommandResult PushAdbFile(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &localPath,
        const std::string &remotePath,
        const std::atomic<bool> *cancel = nullptr
    );

    AdbCommandResult PullAdbFile(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &remotePath,
        const std::string &localPath,
        const std::atomic<bool> *cancel = nullptr
    );
}

#endif // COREDECK_ADB_CLIENT_H

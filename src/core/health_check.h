//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#ifndef COREDECK_HEALTH_CHECK_H
#define COREDECK_HEALTH_CHECK_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "jdk.h"
#include "sdk.h"
#include "sdk_manager.h"

namespace CoreDeck {
    enum class HealthStatus : uint8_t {
        Pending,
        Running,
        Passed,
        Warning,
        Failed,
        Skipped,
    };

    enum class HealthCheckId : uint8_t {
        SdkRoot,
        EmulatorBinary,
        PlatformTools,
        CmdlineTools,
        JdkRuntime,
        ToolsRun,
        Licenses,
        SystemImages,
        DiskSpace,
    };

    enum class HealthFix : uint8_t {
        None,
        InstallSdk,
        InstallCmdlineTools,
        ConfigureJdk,
        AcceptLicenses,
        InstallSystemImage,
    };

    struct HealthCheckResult {
        HealthCheckId Id = HealthCheckId::SdkRoot;
        HealthStatus Status = HealthStatus::Pending;
        std::string Detail;
        HealthFix Fix = HealthFix::None;
    };

    struct HealthCheckProgressData {
        std::mutex Mutex;
        std::vector<HealthCheckResult> Items;
        bool Finished = false;
    };

    struct HealthCheckDeps {
        std::function<bool(const std::string &path)> PathExists;

        std::function<LicenseStatus(const SdkInfo &sdk)> CheckLicenses;

        // Raw "sdkmanager --version" output; interpreted by
        // InterpretSdkManagerVersionOutput.
        std::function<std::string(const SdkInfo &sdk)> SdkManagerVersion;

        std::function<std::size_t(const SdkInfo &sdk)> CountSystemImages;

        // Free bytes at the given directory, or nullopt when it cannot be read.
        std::function<std::optional<std::uint64_t>(const std::string &root)> FreeDiskSpace;
    };

    HealthCheckDeps DefaultHealthCheckDeps();

    const char *HealthCheckLabel(HealthCheckId id);

    const char *HealthStatusLabel(HealthStatus status);

    // The version sdkmanager reported, when its output shows the tools started.
    // Legacy sdkmanager prints "16.0". Current cmdline-tools print
    // "1.0.16261425 (Android CLI)" after a deprecation warning. Empty when the
    // output is an error instead of a version.
    std::optional<std::string> InterpretSdkManagerVersionOutput(const std::string &output);

    HealthStatus OverallHealth(const std::vector<HealthCheckResult> &items);

    void RunHealthChecks(
        const SdkInfo &sdk,
        const JdkInfo &jdk,
        const std::shared_ptr<HealthCheckProgressData> &progress,
        const HealthCheckDeps &deps
    );

    void RunHealthChecks(
        const SdkInfo &sdk,
        const JdkInfo &jdk,
        const std::shared_ptr<HealthCheckProgressData> &progress
    );
}

#endif // COREDECK_HEALTH_CHECK_H

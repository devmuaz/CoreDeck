//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#ifndef COREDECK_SDK_BOOTSTRAP_H
#define COREDECK_SDK_BOOTSTRAP_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "jdk.h"
#include "sdk.h"
#include "system_image.h"

namespace CoreDeck {
    enum class BootstrapStage : uint8_t {
        Idle,
        Preparing,
        DownloadingCmdlineTools,
        Verifying,
        Extracting,
        ResolvingTools,
        AcceptingLicenses,
        InstallingPackages,
        VerifyingInstall,
        Succeeded,
        Failed,
        Cancelled,
    };

    enum class BootstrapError : uint8_t {
        None,
        InvalidInstallRoot,
        InsufficientDiskSpace,
        UnsupportedPlatform,
        NetworkFailed,
        ChecksumMismatch,
        ExtractFailed,
        JdkRequired,
        SdkManagerMissing,
        LicenseCheckFailed,
        LicenseAcceptFailed,
        PackageInstallFailed,
        EmulatorMissingAfterInstall,
        Cancelled,
        Unknown,
    };

    struct CmdlineToolsRelease {
        std::string Version;
        std::string DownloadUrl;
        std::string Sha256;
        std::string FileName;
        std::uint64_t DownloadSize = 0;
    };

    struct BootstrapPlan {
        std::string InstallRoot;
        std::vector<std::string> Packages{"platform-tools", "emulator"};
        bool AcceptLicenses = true;
    };

    struct BootstrapProgressData {
        std::mutex Mutex;
        BootstrapStage Stage = BootstrapStage::Idle;
        float Percent = 0.0F;
        std::string StatusText;
        std::string DetailText;
        BootstrapError Error = BootstrapError::None;
        std::string ErrorDetail;
        bool Finished = false;
        bool Succeeded = false;
        bool CancelRequested = false;
    };

    struct BootstrapDeps {
        std::function<bool(
            const std::string &url,
            const std::string &destPath,
            const std::function<bool(std::uint64_t, std::uint64_t)> &onProgress,
            std::string &error
        )>
            Download;

        std::function<bool(
            const std::string &zipPath,
            const std::string &destDir,
            const std::function<bool(float)> &onProgress,
            std::string &error
        )>
            Extract;

        std::function<std::string(const std::string &path)> FileSha256;

        std::function<SdkInfo(const std::string &sdkPath)> Probe;

        std::function<LicenseStatus(const SdkInfo &sdk)> CheckLicenses;

        std::function<bool(const SdkInfo &sdk)> AcceptLicenses;

        std::function<bool(
            const SdkInfo &sdk,
            const std::string &sdkRoot,
            const std::vector<std::string> &packages,
            const std::shared_ptr<BootstrapProgressData> &progress
        )>
            InstallPackages;
    };

    CmdlineToolsRelease GetBundledCmdlineToolsRelease();

    BootstrapDeps DefaultBootstrapDeps();

    bool BootstrapAndroidSdk(
        const BootstrapPlan &plan,
        const JdkInfo &jdk,
        const std::shared_ptr<BootstrapProgressData> &progress,
        const BootstrapDeps &deps
    );

    bool BootstrapAndroidSdk(
        const BootstrapPlan &plan,
        const JdkInfo &jdk,
        const std::shared_ptr<BootstrapProgressData> &progress
    );

    const char *BootstrapErrorMessage(BootstrapError error);

    const char *BootstrapStageLabel(BootstrapStage stage);

    std::string BootstrapStagingDirectory(const std::string &installRoot);

    constexpr std::uint64_t BOOTSTRAP_REQUIRED_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL;
}

#endif // COREDECK_SDK_BOOTSTRAP_H

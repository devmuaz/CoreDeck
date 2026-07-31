//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#include <algorithm>
#include <filesystem>

#include "sdk_bootstrap.h"
#include "archive.h"
#include "http_download.h"
#include "paths.h"
#include "process.h"
#include "sha256.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        constexpr const char *CMDLINE_TOOLS_VERSION = "15859902";
        constexpr const char *CMDLINE_TOOLS_BASE_URL = "https://dl.google.com/android/repository/";
        constexpr const char *STAGING_DIR_NAME = ".coredeck-bootstrap";

        constexpr float DOWNLOAD_START = 0.02F;
        constexpr float DOWNLOAD_END = 0.55F;
        constexpr float CHECKSUM_END = 0.60F;
        constexpr float EXTRACT_END = 0.72F;
        constexpr float RESOLVE_END = 0.74F;
        constexpr float LICENSE_END = 0.78F;
        constexpr float PACKAGES_END = 0.98F;

        void SetStage(
            const std::shared_ptr<BootstrapProgressData> &progress,
            const BootstrapStage stage,
            const float percent,
            const std::string &status
        ) {
            if (!progress) {
                return;
            }
            std::lock_guard lock(progress->Mutex);
            progress->Stage = stage;
            progress->Percent = percent;
            progress->StatusText = status;
            progress->DetailText.clear();
        }

        void SetDetail(const std::shared_ptr<BootstrapProgressData> &progress, const std::string &detail) {
            if (!progress) {
                return;
            }
            std::lock_guard lock(progress->Mutex);
            progress->DetailText = detail;
        }

        void SetPercent(const std::shared_ptr<BootstrapProgressData> &progress, const float percent) {
            if (!progress) {
                return;
            }
            std::lock_guard lock(progress->Mutex);
            progress->Percent = percent;
        }

        bool IsCancelled(const std::shared_ptr<BootstrapProgressData> &progress) {
            if (!progress) {
                return false;
            }
            std::lock_guard lock(progress->Mutex);
            return progress->CancelRequested;
        }

        bool Finish(
            const std::shared_ptr<BootstrapProgressData> &progress,
            const BootstrapError error,
            const std::string &detail
        ) {
            const bool succeeded = error == BootstrapError::None;

            BootstrapStage finalStage = BootstrapStage::Failed;
            if (succeeded) {
                finalStage = BootstrapStage::Succeeded;
            } else if (error == BootstrapError::Cancelled) {
                finalStage = BootstrapStage::Cancelled;
            }

            if (progress) {
                std::lock_guard lock(progress->Mutex);
                progress->Stage = finalStage;
                progress->Error = error;
                progress->ErrorDetail = detail;
                progress->Finished = true;
                progress->Succeeded = succeeded;
                progress->StatusText = succeeded ? "Android SDK ready." : BootstrapErrorMessage(error);
                if (succeeded) {
                    progress->Percent = 1.0F;
                }
            }
            return succeeded;
        }

        void RemoveStaging(const std::string &installRoot) {
            std::error_code ec;
            std::filesystem::remove_all(BootstrapStagingDirectory(installRoot), ec);
        }

        float Interpolate(const float from, const float to, const float ratio) {
            const float clamped = std::clamp(ratio, 0.0F, 1.0F);
            return from + ((to - from) * clamped);
        }

        void ParseSdkManagerProgress(
            const std::string &line,
            const std::shared_ptr<BootstrapProgressData> &progress
        ) {
            if (!progress || line.empty()) {
                return;
            }

            const auto bracket = line.find('[');
            const auto closeBracket = line.find(']', bracket);
            if (bracket == std::string::npos || closeBracket == std::string::npos) {
                SetDetail(progress, line);
                return;
            }

            auto afterBracket = line.substr(closeBracket + 1);
            const auto start = afterBracket.find_first_not_of(" \t");
            if (start == std::string::npos) {
                return;
            }
            afterBracket = afterBracket.substr(start);

            const auto pctEnd = afterBracket.find('%');
            if (pctEnd == std::string::npos) {
                SetDetail(progress, line);
                return;
            }

            const auto pct = static_cast<float>(std::strtol(afterBracket.substr(0, pctEnd).c_str(), nullptr, 10));
            SetPercent(progress, Interpolate(LICENSE_END, PACKAGES_END, pct / 100.0F));
            SetDetail(progress, line);
        }

        std::string PlatformArchiveName() {
            switch (Paths::GetCurrentPlatform()) {
                case Paths::Platform::Windows:
                    return StrConcat("commandlinetools-win-", CMDLINE_TOOLS_VERSION, "_latest.zip");
                case Paths::Platform::MacOS:
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
                    return StrConcat("commandlinetools-mac_arm64-", CMDLINE_TOOLS_VERSION, "_latest.zip");
#else
                    return StrConcat("commandlinetools-mac_x86_64-", CMDLINE_TOOLS_VERSION, "_latest.zip");
#endif
                case Paths::Platform::Linux:
                    // Google publishes a single Linux archive; the tools themselves are Java.
                    return StrConcat("commandlinetools-linux-", CMDLINE_TOOLS_VERSION, "_latest.zip");
                case Paths::Platform::Unknown:
                default:
                    return {};
            }
        }

        std::string PlatformArchiveSha256() {
            switch (Paths::GetCurrentPlatform()) {
                case Paths::Platform::Windows:
                    return "90ae805d20434428bffcb699c290860f19bb5f66a67e6b330067e3de801fb04a";
                case Paths::Platform::MacOS:
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
                    return "835b62a26162b229b441d1f6d4680383815a270809eb33522c0d480fa5002c4e";
#else
                    return "c5a6378ab5cf7e0d5701921405115befff13e9ff7417fb588389338f8bd050f3";
#endif
                case Paths::Platform::Linux:
                    return "4e4c464f145a7512b57d088ac6c278c03c9eea610886b35a5e0804e74eedf583";
                case Paths::Platform::Unknown:
                default:
                    return {};
            }
        }

        std::uint64_t PlatformArchiveSize() {
            switch (Paths::GetCurrentPlatform()) {
                case Paths::Platform::Windows:
                    return 155655386ULL;
                case Paths::Platform::MacOS:
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
                    return 156083281ULL;
#else
                    return 156281494ULL;
#endif
                case Paths::Platform::Linux:
                    return 181833628ULL;
                case Paths::Platform::Unknown:
                default:
                    return 0ULL;
            }
        }
    }

    CmdlineToolsRelease GetBundledCmdlineToolsRelease() {
        CmdlineToolsRelease release;
        release.Version = CMDLINE_TOOLS_VERSION;
        release.FileName = PlatformArchiveName();
        if (release.FileName.empty()) {
            return release;
        }
        release.DownloadUrl = StrConcat(CMDLINE_TOOLS_BASE_URL, release.FileName);
        release.Sha256 = PlatformArchiveSha256();
        release.DownloadSize = PlatformArchiveSize();

#if !defined(NDEBUG)
        // Debug-only escape hatch for exercising failure paths against a local file server.
        if (const char *urlOverride = std::getenv("COREDECK_CMDLINE_TOOLS_URL")) { // NOLINT(concurrency-mt-unsafe)
            if (*urlOverride) {
                release.DownloadUrl = urlOverride;
                if (const char *shaOverride = std::getenv("COREDECK_CMDLINE_TOOLS_SHA256")) { // NOLINT(concurrency-mt-unsafe)
                    release.Sha256 = shaOverride;
                }
            }
        }
#endif

        return release;
    }

    std::string BootstrapStagingDirectory(const std::string &installRoot) {
        return Paths::JoinPaths({installRoot, STAGING_DIR_NAME});
    }

    const char *BootstrapErrorMessage(const BootstrapError error) {
        switch (error) {
            case BootstrapError::None:
                return "No error.";
            case BootstrapError::InvalidInstallRoot:
                return "The chosen install location cannot be used.";
            case BootstrapError::InsufficientDiskSpace:
                return "Not enough free disk space to install the Android SDK (about 2 GB is needed).";
            case BootstrapError::UnsupportedPlatform:
                return "Google does not publish command-line tools for this platform.";
            case BootstrapError::NetworkFailed:
                return "The download failed. Check your internet connection and try again.";
            case BootstrapError::ChecksumMismatch:
                return "The downloaded archive did not match its expected checksum.";
            case BootstrapError::ExtractFailed:
                return "The downloaded archive could not be extracted.";
            case BootstrapError::JdkRequired:
                return "A JDK 17 or newer is required before the Android SDK can be installed.";
            case BootstrapError::SdkManagerMissing:
                return "The SDK Manager was not found after extracting the command-line tools.";
            case BootstrapError::LicenseCheckFailed:
                return "The Android SDK license state could not be read.";
            case BootstrapError::LicenseAcceptFailed:
                return "The Android SDK licenses were not accepted.";
            case BootstrapError::PackageInstallFailed:
                return "The SDK Manager could not install the required packages.";
            case BootstrapError::EmulatorMissingAfterInstall:
                return "The emulator is still missing after the installation finished.";
            case BootstrapError::Cancelled:
                return "The installation was cancelled.";
            case BootstrapError::Unknown:
            default:
                return "The Android SDK installation failed.";
        }
    }

    const char *BootstrapStageLabel(const BootstrapStage stage) {
        switch (stage) {
            case BootstrapStage::Idle:
                return "Ready";
            case BootstrapStage::Preparing:
                return "Preparing";
            case BootstrapStage::DownloadingCmdlineTools:
                return "Downloading command-line tools";
            case BootstrapStage::Verifying:
                return "Verifying download";
            case BootstrapStage::Extracting:
                return "Extracting";
            case BootstrapStage::ResolvingTools:
                return "Locating tools";
            case BootstrapStage::AcceptingLicenses:
                return "Accepting licenses";
            case BootstrapStage::InstallingPackages:
                return "Installing packages";
            case BootstrapStage::VerifyingInstall:
                return "Verifying installation";
            case BootstrapStage::Succeeded:
                return "Done";
            case BootstrapStage::Failed:
                return "Failed";
            case BootstrapStage::Cancelled:
                return "Cancelled";
            default:
                return "Working";
        }
    }

    BootstrapDeps DefaultBootstrapDeps() {
        BootstrapDeps deps;

        deps.Download = [](
                            const std::string &url,
                            const std::string &destPath,
                            const std::function<bool(std::uint64_t, std::uint64_t)> &onProgress,
                            std::string &error
                        ) {
            const std::string userAgent = StrConcat("CoreDeck/", COREDECK_VERSION);
            return HttpDownloadToFile(url, destPath, userAgent, onProgress, error);
        };

        deps.Extract = [](
                           const std::string &zipPath,
                           const std::string &destDir,
                           const std::function<bool(float)> &onProgress,
                           std::string &error
                       ) {
            return ExtractZip(zipPath, destDir, ExtractOptions{.StripTopLevelDir = true}, onProgress, error);
        };

        deps.FileSha256 = [](const std::string &path) { return Sha256File(path); };

        deps.Probe = [](const std::string &sdkPath) { return ProbeAndroidSdk(sdkPath); };

        deps.CheckLicenses = [](const SdkInfo &sdk) { return CheckSdkLicenses(sdk); };

        deps.AcceptLicenses = [](const SdkInfo &sdk) { return AcceptSdkLicenses(sdk); };

        deps.InstallPackages = [](
                                   const SdkInfo &sdk,
                                   const std::string &sdkRoot,
                                   const std::vector<std::string> &packages,
                                   const std::shared_ptr<BootstrapProgressData> &progress
                               ) {
            if (sdk.SdkManagerPath.empty()) {
                return false;
            }

            std::vector<std::string> args;
            args.reserve(packages.size() + 2);
            args.push_back(StrConcat("--sdk_root=", sdkRoot));
            args.emplace_back("--install");
            for (const auto &package: packages) {
                args.push_back(package);
            }

            StreamCommandArgs(
                sdk.SdkManagerPath,
                args,
                "y\n",
                [&progress](const std::string &line) { ParseSdkManagerProgress(line, progress); },
                sdk.ToolEnv
            );
            return true;
        };

        return deps;
    }

    // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
    bool BootstrapAndroidSdk(
        const BootstrapPlan &plan,
        const JdkInfo &jdk,
        const std::shared_ptr<BootstrapProgressData> &progress,
        const BootstrapDeps &deps
    ) {
        SetStage(progress, BootstrapStage::Preparing, 0.0F, "Preparing...");

        if (plan.InstallRoot.empty()) {
            return Finish(progress, BootstrapError::InvalidInstallRoot, "No install location was provided.");
        }
        if (!jdk.IsFound || !jdk.IsValid) {
            return Finish(progress, BootstrapError::JdkRequired, jdk.VersionString);
        }

        const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();
        if (release.DownloadUrl.empty()) {
            return Finish(progress, BootstrapError::UnsupportedPlatform, Paths::GetPlatformName());
        }

        std::error_code ec;
        std::filesystem::create_directories(plan.InstallRoot, ec);
        if (ec || !std::filesystem::is_directory(plan.InstallRoot)) {
            return Finish(progress, BootstrapError::InvalidInstallRoot, plan.InstallRoot);
        }

        if (const std::filesystem::space_info space = std::filesystem::space(plan.InstallRoot, ec);
            !ec && space.available > 0 && space.available < BOOTSTRAP_REQUIRED_BYTES) {
            return Finish(progress, BootstrapError::InsufficientDiskSpace, FormatFileSize(space.available));
        }

        const std::string staging = BootstrapStagingDirectory(plan.InstallRoot);
        RemoveStaging(plan.InstallRoot);
        std::filesystem::create_directories(staging, ec);
        if (ec) {
            return Finish(progress, BootstrapError::InvalidInstallRoot, staging);
        }

        if (IsCancelled(progress)) {
            RemoveStaging(plan.InstallRoot);
            return Finish(progress, BootstrapError::Cancelled, "");
        }

        // Download
        SetStage(progress, BootstrapStage::DownloadingCmdlineTools, DOWNLOAD_START, "Downloading command-line tools...");
        const std::string archivePath = Paths::JoinPaths({staging, release.FileName});
        std::string error;
        const bool downloaded = deps.Download(
            release.DownloadUrl,
            archivePath,
            [&progress, &release](const std::uint64_t received, const std::uint64_t total) {
                if (IsCancelled(progress)) {
                    return false;
                }
                const std::uint64_t expected = total > 0 ? total : release.DownloadSize;
                if (expected > 0) {
                    const float ratio = static_cast<float>(received) / static_cast<float>(expected);
                    SetPercent(progress, Interpolate(DOWNLOAD_START, DOWNLOAD_END, ratio));
                }
                SetDetail(progress, StrConcat(FormatFileSize(received), " downloaded"));
                return true;
            },
            error
        );

        if (!downloaded) {
            RemoveStaging(plan.InstallRoot);
            if (IsCancelled(progress)) {
                return Finish(progress, BootstrapError::Cancelled, "");
            }
            return Finish(progress, BootstrapError::NetworkFailed, error);
        }

        if (IsCancelled(progress)) {
            RemoveStaging(plan.InstallRoot);
            return Finish(progress, BootstrapError::Cancelled, "");
        }

        // Checksum
        SetStage(progress, BootstrapStage::Verifying, DOWNLOAD_END, "Verifying download...");
        if (!release.Sha256.empty()) {
            const std::string actual = deps.FileSha256(archivePath);
            if (!EqualsIgnoreCaseHex(actual, release.Sha256)) {
                RemoveStaging(plan.InstallRoot);
                return Finish(
                    progress,
                    BootstrapError::ChecksumMismatch,
                    StrConcat("expected ", release.Sha256, ", got ", actual.empty() ? "nothing" : actual)
                );
            }
        }
        SetPercent(progress, CHECKSUM_END);

        // Extract
        SetStage(progress, BootstrapStage::Extracting, CHECKSUM_END, "Extracting command-line tools...");
        const std::string extractDir = Paths::JoinPaths({staging, "extracted"});
        const bool extracted = deps.Extract(
            archivePath,
            extractDir,
            [&progress](const float ratio) {
                if (IsCancelled(progress)) {
                    return false;
                }
                SetPercent(progress, Interpolate(CHECKSUM_END, EXTRACT_END, ratio));
                return true;
            },
            error
        );

        if (!extracted) {
            RemoveStaging(plan.InstallRoot);
            if (IsCancelled(progress)) {
                return Finish(progress, BootstrapError::Cancelled, "");
            }
            return Finish(progress, BootstrapError::ExtractFailed, error);
        }

        // Move into place: <root>/cmdline-tools/latest
        const std::string cmdlineToolsRoot = Paths::JoinPaths({plan.InstallRoot, "cmdline-tools"});
        const std::string latestDir = Paths::JoinPaths({cmdlineToolsRoot, "latest"});
        std::filesystem::create_directories(cmdlineToolsRoot, ec);
        std::filesystem::remove_all(latestDir, ec);
        std::filesystem::rename(extractDir, latestDir, ec);
        if (ec) {
            // Different filesystems: fall back to a copy.
            ec.clear();
            std::filesystem::copy(
                extractDir,
                latestDir,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                ec
            );
            if (ec) {
                RemoveStaging(plan.InstallRoot);
                return Finish(progress, BootstrapError::ExtractFailed, ec.message());
            }
        }
        RemoveStaging(plan.InstallRoot);

        if (IsCancelled(progress)) {
            return Finish(progress, BootstrapError::Cancelled, "");
        }

        // Resolve the freshly installed tools
        SetStage(progress, BootstrapStage::ResolvingTools, EXTRACT_END, "Locating the SDK Manager...");
        SdkInfo sdk = deps.Probe(plan.InstallRoot);
        ApplyJdkToSdk(sdk, jdk);
        if (sdk.SdkManagerPath.empty()) {
            return Finish(progress, BootstrapError::SdkManagerMissing, latestDir);
        }
        SetPercent(progress, RESOLVE_END);

        // Licenses
        if (plan.AcceptLicenses) {
            SetStage(progress, BootstrapStage::AcceptingLicenses, RESOLVE_END, "Accepting SDK licenses...");
            const LicenseStatus status = deps.CheckLicenses(sdk);
            if (status == LicenseStatus::CheckFailed) {
                return Finish(progress, BootstrapError::LicenseCheckFailed, "");
            }
            if (status == LicenseStatus::SomeUnaccepted && !deps.AcceptLicenses(sdk)) {
                return Finish(progress, BootstrapError::LicenseAcceptFailed, "");
            }
        }
        SetPercent(progress, LICENSE_END);

        if (IsCancelled(progress)) {
            return Finish(progress, BootstrapError::Cancelled, "");
        }

        // Packages
        if (!plan.Packages.empty()) {
            SetStage(progress, BootstrapStage::InstallingPackages, LICENSE_END, "Installing platform tools and emulator...");
            if (!deps.InstallPackages(sdk, plan.InstallRoot, plan.Packages, progress)) {
                return Finish(progress, BootstrapError::PackageInstallFailed, "");
            }
        }

        // Verify
        SetStage(progress, BootstrapStage::VerifyingInstall, PACKAGES_END, "Verifying installation...");
        const SdkInfo verified = deps.Probe(plan.InstallRoot);
        if (!verified.IsFound) {
            return Finish(progress, BootstrapError::EmulatorMissingAfterInstall, verified.EmulatorPath);
        }

        return Finish(progress, BootstrapError::None, "");
    }

    bool BootstrapAndroidSdk(
        const BootstrapPlan &plan,
        const JdkInfo &jdk,
        const std::shared_ptr<BootstrapProgressData> &progress
    ) {
        return BootstrapAndroidSdk(plan, jdk, progress, DefaultBootstrapDeps());
    }
}

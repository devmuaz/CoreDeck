//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#include "health_check.h"
#include "paths.h"
#include "process.h"
#include "sdk_bootstrap.h"
#include "system_image.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        constexpr HealthCheckId CHECK_ORDER[] = {
            HealthCheckId::SdkRoot,
            HealthCheckId::EmulatorBinary,
            HealthCheckId::PlatformTools,
            HealthCheckId::CmdlineTools,
            HealthCheckId::JdkRuntime,
            HealthCheckId::ToolsRun,
            HealthCheckId::Licenses,
            HealthCheckId::SystemImages,
            HealthCheckId::DiskSpace,
        };

        void SetItem(
            const std::shared_ptr<HealthCheckProgressData> &progress,
            const std::size_t index,
            const HealthStatus status,
            const std::string &detail = {},
            const HealthFix fix = HealthFix::None
        ) {
            if (!progress) {
                return;
            }
            std::lock_guard lock(progress->Mutex);
            if (index >= progress->Items.size()) {
                return;
            }
            progress->Items[index].Status = status;
            progress->Items[index].Detail = detail;
            progress->Items[index].Fix = fix;
        }

        std::string TrimLine(std::string line) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            const auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos) {
                return {};
            }
            return line.substr(start);
        }

        bool IsSdkManagerNotice(const std::string &line) {
            return line.starts_with("WARNING:") || line.starts_with("Warning:") ||
                   line.starts_with("The 'android' binary") || line.starts_with("To learn more about");
        }

        std::string FirstDiagnosticLine(const std::string &text) {
            std::istringstream stream(text);
            std::string line;
            std::string first;
            while (std::getline(stream, line)) {
                line = TrimLine(std::move(line));
                if (line.empty()) {
                    continue;
                }
                if (first.empty()) {
                    first = line;
                }
                if (!IsSdkManagerNotice(line)) {
                    return line;
                }
            }
            return first;
        }

        bool IsVersionToken(const std::string &text) {
            if (text.empty() || !static_cast<bool>(std::isdigit(static_cast<unsigned char>(text.front())))) {
                return false;
            }
            if (text.back() == '.') {
                return false;
            }
            return std::ranges::all_of(text, [](const char c) {
                return static_cast<bool>(std::isdigit(static_cast<unsigned char>(c))) || c == '.';
            });
        }

        HealthCheckResult CheckSdkRoot(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::SdkRoot};
            if (sdk.SdkPath.empty()) {
                result.Status = HealthStatus::Failed;
                result.Detail = "No Android SDK location is configured.";
                result.Fix = HealthFix::InstallSdk;
                return result;
            }
            if (!deps.PathExists(sdk.SdkPath)) {
                result.Status = HealthStatus::Failed;
                result.Detail = StrConcat("The configured SDK folder does not exist: ", sdk.SdkPath);
                result.Fix = HealthFix::InstallSdk;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = sdk.SdkPath;
            return result;
        }

        HealthCheckResult CheckEmulatorBinary(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::EmulatorBinary};
            if (sdk.EmulatorPath.empty() || !deps.PathExists(sdk.EmulatorPath)) {
                result.Status = HealthStatus::Failed;
                result.Detail = "The emulator binary was not found in this SDK.";
                result.Fix = HealthFix::InstallSdk;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = sdk.EmulatorPath;
            return result;
        }

        HealthCheckResult CheckPlatformTools(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::PlatformTools};
            const std::string adbPath = Paths::JoinPaths(
                {sdk.SdkPath, "platform-tools", "adb" + Paths::GetExecutableExtension()}
            );
            if (!deps.PathExists(adbPath)) {
                result.Status = HealthStatus::Warning;
                result.Detail = "adb was not found under platform-tools.";
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = adbPath;
            return result;
        }

        HealthCheckResult CheckCmdlineTools(const SdkInfo &sdk) {
            HealthCheckResult result{.Id = HealthCheckId::CmdlineTools};
            if (sdk.AvdManagerPath.empty() || sdk.SdkManagerPath.empty()) {
                result.Status = HealthStatus::Failed;
                result.Detail = "avdmanager and/or sdkmanager are missing from this SDK.";
                result.Fix = HealthFix::InstallCmdlineTools;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = "avdmanager and sdkmanager are available.";
            return result;
        }

        HealthCheckResult CheckJdkRuntime(const JdkInfo &jdk) {
            HealthCheckResult result{.Id = HealthCheckId::JdkRuntime};
            if (!jdk.IsFound) {
                result.Status = HealthStatus::Warning;
                result.Detail = "No JDK detected. The command-line tools will use whatever 'java' is on your PATH.";
                result.Fix = HealthFix::ConfigureJdk;
                return result;
            }
            if (!jdk.IsValid) {
                result.Status = HealthStatus::Failed;
                result.Detail = StrConcat(
                    "Found ",
                    jdk.VersionString.empty() ? "an unknown Java version" : jdk.VersionString,
                    ", which is older than JDK ",
                    std::to_string(JDK_MINIMUM_MAJOR),
                    "."
                );
                result.Fix = HealthFix::ConfigureJdk;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = jdk.VersionString.empty() ? jdk.JavaHome : jdk.VersionString;
            return result;
        }

        HealthCheckResult CheckToolsRun(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::ToolsRun};
            if (sdk.SdkManagerPath.empty()) {
                result.Status = HealthStatus::Skipped;
                result.Detail = "sdkmanager is not installed.";
                return result;
            }

            const std::string output = deps.SdkManagerVersion(sdk);
            if (const std::optional<std::string> version = InterpretSdkManagerVersionOutput(output)) {
                result.Status = HealthStatus::Passed;
                result.Detail = StrConcat("sdkmanager ", *version);
                return result;
            }

            result.Status = HealthStatus::Failed;
            const std::string firstLine = FirstDiagnosticLine(output);
            result.Detail = firstLine.empty()
                                ? "sdkmanager did not produce any output. Check the Java setup."
                                : StrConcat("sdkmanager did not run correctly: ", firstLine);
            result.Fix = HealthFix::ConfigureJdk;
            return result;
        }

        HealthCheckResult CheckLicensesAccepted(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::Licenses};
            if (sdk.SdkManagerPath.empty()) {
                result.Status = HealthStatus::Skipped;
                result.Detail = "sdkmanager is not installed.";
                return result;
            }

            switch (deps.CheckLicenses(sdk)) {
                case LicenseStatus::AllAccepted:
                    result.Status = HealthStatus::Passed;
                    result.Detail = "All SDK package licenses are accepted.";
                    break;
                case LicenseStatus::SomeUnaccepted:
                    result.Status = HealthStatus::Failed;
                    result.Detail = "Some SDK package licenses have not been accepted.";
                    result.Fix = HealthFix::AcceptLicenses;
                    break;
                case LicenseStatus::CheckFailed:
                default:
                    result.Status = HealthStatus::Warning;
                    result.Detail = "The SDK license state could not be read.";
                    break;
            }
            return result;
        }

        HealthCheckResult CheckSystemImages(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::SystemImages};
            const std::size_t count = deps.CountSystemImages(sdk);
            if (count == 0) {
                result.Status = HealthStatus::Warning;
                result.Detail = "No system images are installed yet, so no AVD can boot.";
                result.Fix = HealthFix::InstallSystemImage;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = StrConcat(std::to_string(count), count == 1 ? " system image" : " system images", " installed.");
            return result;
        }

        HealthCheckResult CheckDiskSpace(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::DiskSpace};
            const std::optional<std::uint64_t> freeBytes = deps.FreeDiskSpace(sdk.SdkPath);
            if (!freeBytes.has_value()) {
                result.Status = HealthStatus::Skipped;
                result.Detail = "The free disk space could not be determined.";
                return result;
            }
            if (*freeBytes < BOOTSTRAP_REQUIRED_BYTES) {
                result.Status = HealthStatus::Warning;
                result.Detail = StrConcat(
                    "Only ",
                    FormatFileSize(*freeBytes),
                    " is free. About ",
                    FormatFileSize(BOOTSTRAP_REQUIRED_BYTES),
                    " is recommended for downloads."
                );
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = StrConcat(FormatFileSize(*freeBytes), " free.");
            return result;
        }

        HealthCheckResult SkippedResult(const HealthCheckId id) {
            return {
                .Id = id,
                .Status = HealthStatus::Skipped,
                .Detail = "Needs a configured Android SDK.",
                .Fix = HealthFix::None,
            };
        }

        HealthCheckResult RunCheck(
            const HealthCheckId id,
            const SdkInfo &sdk,
            const JdkInfo &jdk,
            const bool sdkAvailable,
            const HealthCheckDeps &deps
        ) {
            switch (id) {
                case HealthCheckId::SdkRoot:
                    return CheckSdkRoot(sdk, deps);
                case HealthCheckId::EmulatorBinary:
                    return sdkAvailable ? CheckEmulatorBinary(sdk, deps) : SkippedResult(id);
                case HealthCheckId::PlatformTools:
                    return sdkAvailable ? CheckPlatformTools(sdk, deps) : SkippedResult(id);
                case HealthCheckId::CmdlineTools:
                    return sdkAvailable ? CheckCmdlineTools(sdk) : SkippedResult(id);
                case HealthCheckId::JdkRuntime:
                    return CheckJdkRuntime(jdk);
                case HealthCheckId::ToolsRun:
                    return sdkAvailable ? CheckToolsRun(sdk, deps) : SkippedResult(id);
                case HealthCheckId::Licenses:
                    return sdkAvailable ? CheckLicensesAccepted(sdk, deps) : SkippedResult(id);
                case HealthCheckId::SystemImages:
                    return sdkAvailable ? CheckSystemImages(sdk, deps) : SkippedResult(id);
                case HealthCheckId::DiskSpace:
                default:
                    return sdkAvailable ? CheckDiskSpace(sdk, deps) : SkippedResult(id);
            }
        }
    }

    HealthCheckDeps DefaultHealthCheckDeps() {
        HealthCheckDeps deps;

        deps.PathExists = [](const std::string &path) {
            std::error_code ec;
            return std::filesystem::exists(path, ec);
        };

        deps.CheckLicenses = [](const SdkInfo &sdk) { return CheckSdkLicenses(sdk); };

        deps.SdkManagerVersion = [](const SdkInfo &sdk) {
            return RunCommandArgs(sdk.SdkManagerPath, {"--version"}, "", sdk.ToolEnv);
        };

        deps.CountSystemImages = [](const SdkInfo &sdk) { return ListSystemImages(sdk).size(); };

        deps.FreeDiskSpace = [](const std::string &root) -> std::optional<std::uint64_t> {
            std::error_code ec;
            const std::filesystem::space_info space = std::filesystem::space(root, ec);
            if (ec || space.available == static_cast<std::uintmax_t>(-1)) {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(space.available);
        };

        return deps;
    }

    const char *HealthCheckLabel(const HealthCheckId id) {
        switch (id) {
            case HealthCheckId::SdkRoot:
                return "Android SDK location";
            case HealthCheckId::EmulatorBinary:
                return "Emulator binary";
            case HealthCheckId::PlatformTools:
                return "Platform tools (adb)";
            case HealthCheckId::CmdlineTools:
                return "Command-line tools (avdmanager, sdkmanager)";
            case HealthCheckId::JdkRuntime:
                return "Java runtime (JDK 17 or newer)";
            case HealthCheckId::ToolsRun:
                return "Command-line tools run";
            case HealthCheckId::Licenses:
                return "SDK licenses";
            case HealthCheckId::SystemImages:
                return "System images";
            case HealthCheckId::DiskSpace:
            default:
                return "Free disk space";
        }
    }

    const char *HealthStatusLabel(const HealthStatus status) {
        switch (status) {
            case HealthStatus::Pending:
                return "Pending";
            case HealthStatus::Running:
                return "Checking...";
            case HealthStatus::Passed:
                return "Passed";
            case HealthStatus::Warning:
                return "Warning";
            case HealthStatus::Failed:
                return "Failed";
            case HealthStatus::Skipped:
            default:
                return "Skipped";
        }
    }

    std::optional<std::string> InterpretSdkManagerVersionOutput(const std::string &output) {
        constexpr const char *ANDROID_CLI_SUFFIX = " (Android CLI)";

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimLine(std::move(line));
            if (line.empty() || IsSdkManagerNotice(line)) {
                continue;
            }

            if (IsVersionToken(line)) {
                return line;
            }

            if (line.ends_with(ANDROID_CLI_SUFFIX)) {
                std::string version = line.substr(0, line.size() - std::char_traits<char>::length(ANDROID_CLI_SUFFIX));
                if (IsVersionToken(version)) {
                    return version;
                }
                if (version == "unknown") {
                    return std::string("Android CLI");
                }
            }
        }
        return std::nullopt;
    }

    HealthStatus OverallHealth(const std::vector<HealthCheckResult> &items) {
        HealthStatus overall = HealthStatus::Passed;
        for (const auto &item: items) {
            if (item.Status == HealthStatus::Failed) {
                return HealthStatus::Failed;
            }
            if (item.Status == HealthStatus::Warning) {
                overall = HealthStatus::Warning;
            }
        }
        return overall;
    }

    void RunHealthChecks(
        const SdkInfo &sdk,
        const JdkInfo &jdk,
        const std::shared_ptr<HealthCheckProgressData> &progress,
        const HealthCheckDeps &deps
    ) {
        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->Items.clear();
            progress->Items.reserve(std::size(CHECK_ORDER));
            for (const HealthCheckId id: CHECK_ORDER) {
                progress->Items.push_back({.Id = id, .Status = HealthStatus::Pending});
            }
            progress->Finished = false;
        }

        const bool sdkAvailable = !sdk.SdkPath.empty() && deps.PathExists(sdk.SdkPath);

        for (std::size_t i = 0; i < std::size(CHECK_ORDER); ++i) {
            SetItem(progress, i, HealthStatus::Running);
            const HealthCheckResult result = RunCheck(CHECK_ORDER[i], sdk, jdk, sdkAvailable, deps);
            SetItem(progress, i, result.Status, result.Detail, result.Fix);
        }

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->Finished = true;
        }
    }

    void RunHealthChecks(
        const SdkInfo &sdk,
        const JdkInfo &jdk,
        const std::shared_ptr<HealthCheckProgressData> &progress
    ) {
        RunHealthChecks(sdk, jdk, progress, DefaultHealthCheckDeps());
    }
}

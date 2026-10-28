//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#include <filesystem>
#include <sstream>
#include <utility>

#include "core/health_check.h"
#include "platform/i18n.h"
#include "platform/paths.h"
#include "core/system_image.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        constexpr HealthCheckId CHECK_ORDER[] = {
            HealthCheckId::SdkRoot,
            HealthCheckId::EmulatorBinary,
            HealthCheckId::PlatformTools,
            HealthCheckId::BuildTools,
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
            const std::scoped_lock lock(progress->Mutex);
            if (index >= progress->Items.size()) {
                return;
            }
            progress->Items.at(static_cast<std::size_t>(index)).Status = status;
            progress->Items.at(static_cast<std::size_t>(index)).Detail = detail;
            progress->Items.at(static_cast<std::size_t>(index)).Fix = fix;
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

        enum class VersionPart : uint8_t {
            Numeric,
            Prerelease,
        };

        bool IsAllowedPartChar(const unsigned char c, const VersionPart part) {
            if (static_cast<bool>(std::isdigit(c))) {
                return true;
            }
            if (part == VersionPart::Numeric) {
                return false;
            }
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        }

        bool IsDottedPart(const std::string_view text, const VersionPart part) {
            bool afterSeparator = true;
            for (const char c: text) {
                if (c == '.') {
                    if (afterSeparator) {
                        return false;
                    }
                    afterSeparator = true;
                    continue;
                }
                if (!IsAllowedPartChar(static_cast<unsigned char>(c), part)) {
                    return false;
                }
                afterSeparator = false;
            }
            return !afterSeparator;
        }

        bool IsVersionToken(const std::string &text) {
            const std::string_view view(text);
            const auto hyphen = view.find('-');
            const std::string_view release = hyphen == std::string_view::npos ? view : view.substr(0, hyphen);
            if (!IsDottedPart(release, VersionPart::Numeric)) {
                return false;
            }
            if (hyphen == std::string_view::npos) {
                return true;
            }
            if (view.find('-', hyphen + 1) != std::string_view::npos) {
                return false;
            }
            return IsDottedPart(view.substr(hyphen + 1), VersionPart::Prerelease);
        }

        HealthCheckResult CheckSdkRoot(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::SdkRoot};
            if (sdk.SdkPath.empty()) {
                result.Status = HealthStatus::Failed;
                result.Detail = Platform::TrFormat("No Android SDK location is configured.");
                result.Fix = HealthFix::InstallSdk;
                return result;
            }
            if (!deps.PathExists(sdk.SdkPath)) {
                result.Status = HealthStatus::Failed;
                result.Detail = Platform::TrFormat("The configured SDK folder does not exist: {0}", sdk.SdkPath);
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
                result.Detail = Platform::TrFormat("The emulator binary was not found in this SDK.");
                result.Fix = HealthFix::InstallSdk;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = sdk.EmulatorPath;
            return result;
        }

        HealthCheckResult CheckPlatformTools(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::PlatformTools};
            const std::string adbPath = Platform::JoinPaths(
                {sdk.SdkPath, "platform-tools", Platform::WithExecutableExtension("adb")}
            );
            if (!deps.PathExists(adbPath)) {
                result.Status = HealthStatus::Warning;
                result.Detail = Platform::TrFormat("adb was not found under platform-tools.");
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = adbPath;
            return result;
        }

        bool CmdlineToolMissing(const std::string &path, const HealthCheckDeps &deps) {
            return path.empty() || !deps.PathExists(path);
        }

        HealthCheckResult CheckBuildTools(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::BuildTools};
            const bool aapt2Missing = CmdlineToolMissing(sdk.Aapt2Path, deps);
            const bool zipalignMissing = CmdlineToolMissing(sdk.ZipalignPath, deps);
            const bool apksignerMissing = CmdlineToolMissing(sdk.ApksignerPath, deps);
            if (aapt2Missing || zipalignMissing || apksignerMissing) {
                result.Status = HealthStatus::Failed;
                result.Detail = Platform::TrFormat("aapt2, zipalign, or apksigner were not found under build-tools.");
                result.Fix = HealthFix::InstallBuildTools;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = Platform::TrFormat("aapt2, zipalign, and apksigner are available.");
            return result;
        }

        HealthCheckResult CheckCmdlineTools(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::CmdlineTools};
            const bool avdMissing = CmdlineToolMissing(sdk.AvdManagerPath, deps);
            const bool sdkManagerMissing = CmdlineToolMissing(sdk.SdkManagerPath, deps);
            const bool analyzerMissing = CmdlineToolMissing(sdk.ApkAnalyzerPath, deps);
            if (avdMissing || sdkManagerMissing || analyzerMissing) {
                result.Status = HealthStatus::Failed;
                result.Detail = Platform::TrFormat("avdmanager, sdkmanager, and/or apkanalyzer are missing from this SDK.");
                result.Fix = HealthFix::InstallCmdlineTools;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = Platform::TrFormat("avdmanager, sdkmanager, and apkanalyzer are available.");
            return result;
        }

        HealthCheckResult CheckJdkRuntime(const JdkInfo &jdk) {
            HealthCheckResult result{.Id = HealthCheckId::JdkRuntime};
            if (!jdk.IsFound) {
                result.Status = HealthStatus::Warning;
                result.Detail = Platform::TrFormat("No JDK detected. The command-line tools will use whatever 'java' is on your PATH.");
                result.Fix = HealthFix::ConfigureJdk;
                return result;
            }
            if (!jdk.IsValid) {
                result.Status = HealthStatus::Failed;
                const std::string version = jdk.VersionString.empty() ? Platform::TrFormat("an unknown Java version") : jdk.VersionString;
                result.Detail = Platform::TrFormat("Found {0}, which is older than JDK {1}.", version, std::to_string(JDK_MINIMUM_MAJOR));
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
                result.Detail = Platform::TrFormat("sdkmanager is not installed.");
                return result;
            }

            if (!deps.PathExists(sdk.SdkManagerPath)) {
                result.Status = HealthStatus::Failed;
                result.Detail = Platform::TrFormat("sdkmanager is missing from this SDK.");
                result.Fix = HealthFix::InstallCmdlineTools;
                return result;
            }

            const std::string output = deps.SdkManagerVersion(sdk);
            if (const std::optional<std::string> version = InterpretSdkManagerVersionOutput(output)) {
                result.Status = HealthStatus::Passed;
                result.Detail = Platform::TrFormat("sdkmanager {0}", *version);
                return result;
            }

            result.Status = HealthStatus::Failed;
            const std::string firstLine = FirstDiagnosticLine(output);
            result.Detail = firstLine.empty()
                                ? Platform::TrFormat("sdkmanager did not produce any output. Check the Java setup.")
                                : Platform::TrFormat("sdkmanager did not run correctly: {0}", firstLine);
            result.Fix = HealthFix::ConfigureJdk;
            return result;
        }

        HealthCheckResult CheckLicensesAccepted(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::Licenses};
            if (sdk.SdkManagerPath.empty()) {
                result.Status = HealthStatus::Skipped;
                result.Detail = Platform::TrFormat("sdkmanager is not installed.");
                return result;
            }

            switch (deps.CheckLicenses(sdk)) {
                case LicenseStatus::AllAccepted:
                    result.Status = HealthStatus::Passed;
                    result.Detail = Platform::TrFormat("All SDK package licenses are accepted.");
                    break;
                case LicenseStatus::SomeUnaccepted:
                    result.Status = HealthStatus::Failed;
                    result.Detail = Platform::TrFormat("Some SDK package licenses have not been accepted.");
                    result.Fix = HealthFix::AcceptLicenses;
                    break;
                case LicenseStatus::CheckFailed:
                default:
                    result.Status = HealthStatus::Warning;
                    result.Detail = Platform::TrFormat("The SDK license state could not be read.");
                    break;
            }
            return result;
        }

        HealthCheckResult CheckSystemImages(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::SystemImages};
            const std::size_t count = deps.CountSystemImages(sdk);
            if (count == 0) {
                result.Status = HealthStatus::Warning;
                result.Detail = Platform::TrFormat("No system images are installed yet, so no AVD can boot.");
                result.Fix = HealthFix::InstallSystemImage;
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = Platform::TrFormatN(
                "{0} system image installed.",
                "{0} system images installed.",
                static_cast<int>(count),
                std::to_string(count)
            );
            return result;
        }

        HealthCheckResult CheckDiskSpace(const SdkInfo &sdk, const HealthCheckDeps &deps) {
            HealthCheckResult result{.Id = HealthCheckId::DiskSpace};
            const std::optional<std::uint64_t> freeBytes = deps.FreeDiskSpace(sdk.SdkPath);
            if (!freeBytes.has_value()) {
                result.Status = HealthStatus::Skipped;
                result.Detail = Platform::TrFormat("The free disk space could not be determined.");
                return result;
            }
            if (*freeBytes < BOOTSTRAP_REQUIRED_BYTES) {
                result.Status = HealthStatus::Warning;
                result.Detail = Platform::TrFormat(
                    "Only {0} is free. About {1} is recommended for downloads.",
                    Platform::FormatFileSize(*freeBytes),
                    Platform::FormatFileSize(BOOTSTRAP_REQUIRED_BYTES)
                );
                return result;
            }
            result.Status = HealthStatus::Passed;
            result.Detail = Platform::TrFormat("{0} free.", Platform::FormatFileSize(*freeBytes));
            return result;
        }

        HealthCheckResult SkippedResult(const HealthCheckId id) {
            return {
                .Id = id,
                .Status = HealthStatus::Skipped,
                .Detail = Platform::TrFormat("Needs a configured Android SDK."),
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
                case HealthCheckId::BuildTools:
                    return sdkAvailable ? CheckBuildTools(sdk, deps) : SkippedResult(id);
                case HealthCheckId::CmdlineTools:
                    return sdkAvailable ? CheckCmdlineTools(sdk, deps) : SkippedResult(id);
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
            const auto output = RunSdkManager(sdk, {"--version"});
            return output.value_or("");
        };

        deps.CountSystemImages = [](const SdkInfo &sdk) { return ListSystemImages(sdk).size(); };

        deps.FreeDiskSpace = [](const std::string &root) -> std::optional<std::uint64_t> {
            std::error_code ec;
            const std::filesystem::space_info space = std::filesystem::space(root, ec);
            if (ec || std::cmp_equal(space.available, static_cast<std::uintmax_t>(-1))) {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(space.available);
        };

        return deps;
    }

    const char *HealthCheckLabel(const HealthCheckId id) {
        switch (id) {
            case HealthCheckId::SdkRoot:
                return Platform::Tr("Android SDK location");
            case HealthCheckId::EmulatorBinary:
                return Platform::Tr("Emulator binary");
            case HealthCheckId::PlatformTools:
                return Platform::Tr("Platform tools (adb)");
            case HealthCheckId::BuildTools:
                return Platform::Tr("Build tools (aapt2, zipalign, apksigner)");
            case HealthCheckId::CmdlineTools:
                return Platform::Tr("Command-line tools (avdmanager, sdkmanager, apkanalyzer)");
            case HealthCheckId::JdkRuntime:
                return Platform::Tr("Java runtime (JDK 17 or newer)");
            case HealthCheckId::ToolsRun:
                return Platform::Tr("Command-line tools run");
            case HealthCheckId::Licenses:
                return Platform::Tr("SDK licenses");
            case HealthCheckId::SystemImages:
                return Platform::Tr("System images");
            case HealthCheckId::DiskSpace:
            default:
                return Platform::Tr("Free disk space");
        }
    }

    const char *HealthStatusLabel(const HealthStatus status) {
        switch (status) {
            case HealthStatus::Pending:
                return Platform::Tr("Pending");
            case HealthStatus::Running:
                return Platform::Tr("Checking...");
            case HealthStatus::Passed:
                return Platform::Tr("Passed");
            case HealthStatus::Warning:
                return Platform::Tr("Warning");
            case HealthStatus::Failed:
                return Platform::Tr("Failed");
            case HealthStatus::Skipped:
            default:
                return Platform::Tr("Skipped");
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
            const std::scoped_lock lock(progress->Mutex);
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
            const std::scoped_lock lock(progress->Mutex);
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

#include <catch2/catch_test_macros.hpp>

#include "core/health_check.h"

using namespace CoreDeck;

namespace {
    SdkInfo FullSdk() {
        SdkInfo sdk;
        sdk.SdkPath = "/sdk";
        sdk.EmulatorPath = "/sdk/emulator/emulator";
        sdk.AvdManagerPath = "/sdk/cmdline-tools/latest/bin/avdmanager";
        sdk.SdkManagerPath = "/sdk/cmdline-tools/latest/bin/sdkmanager";
        sdk.ApkAnalyzerPath = "/sdk/cmdline-tools/latest/bin/apkanalyzer";
        sdk.IsFound = true;
        return sdk;
    }

    JdkInfo ValidJdk() {
        JdkInfo jdk;
        jdk.JavaHome = "/jdk";
        jdk.VersionString = "openjdk version \"17.0.2\"";
        jdk.MajorVersion = 17;
        jdk.IsFound = true;
        jdk.IsValid = true;
        return jdk;
    }

    HealthCheckDeps AllGoodDeps() {
        HealthCheckDeps deps;
        deps.PathExists = [](const std::string &) { return true; };
        deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::AllAccepted; };
        deps.SdkManagerVersion = [](const SdkInfo &) { return std::string("16.0\n"); };
        deps.CountSystemImages = [](const SdkInfo &) { return static_cast<std::size_t>(2); };
        deps.FreeDiskSpace = [](const std::string &) {
            return std::optional<std::uint64_t>(10ULL * 1024ULL * 1024ULL * 1024ULL);
        };
        return deps;
    }

    std::vector<HealthCheckResult> Run(const SdkInfo &sdk, const JdkInfo &jdk, const HealthCheckDeps &deps) {
        auto progress = std::make_shared<HealthCheckProgressData>();
        RunHealthChecks(sdk, jdk, progress, deps);

        std::lock_guard lock(progress->Mutex);
        REQUIRE(progress->Finished);
        return progress->Items;
    }

    const HealthCheckResult &Find(const std::vector<HealthCheckResult> &items, const HealthCheckId id) {
        for (const auto &item: items) {
            if (item.Id == id) {
                return item;
            }
        }
        FAIL("check not found in report");
        static HealthCheckResult unreachable;
        return unreachable;
    }
}

TEST_CASE("RunHealthChecks reports all green for a complete setup", "[health-check]") {
    const auto items = Run(FullSdk(), ValidJdk(), AllGoodDeps());

    REQUIRE(items.size() == 9);
    for (const auto &item: items) {
        INFO(HealthCheckLabel(item.Id));
        REQUIRE(item.Status == HealthStatus::Passed);
    }
    REQUIRE(OverallHealth(items) == HealthStatus::Passed);
}

TEST_CASE("RunHealthChecks fails the SDK root and skips dependent checks when no SDK is configured", "[health-check]") {
    const auto items = Run(SdkInfo{}, ValidJdk(), AllGoodDeps());

    const auto &root = Find(items, HealthCheckId::SdkRoot);
    REQUIRE(root.Status == HealthStatus::Failed);
    REQUIRE(root.Fix == HealthFix::InstallSdk);

    REQUIRE(Find(items, HealthCheckId::EmulatorBinary).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::CmdlineTools).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::ToolsRun).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::Licenses).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::SystemImages).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::DiskSpace).Status == HealthStatus::Skipped);

    // The JDK does not depend on the SDK and is still checked.
    REQUIRE(Find(items, HealthCheckId::JdkRuntime).Status == HealthStatus::Passed);

    REQUIRE(OverallHealth(items) == HealthStatus::Failed);
}

TEST_CASE("RunHealthChecks flags unaccepted licenses with an accept fix", "[health-check]") {
    HealthCheckDeps deps = AllGoodDeps();
    deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::SomeUnaccepted; };

    const auto items = Run(FullSdk(), ValidJdk(), deps);

    const auto &licenses = Find(items, HealthCheckId::Licenses);
    REQUIRE(licenses.Status == HealthStatus::Failed);
    REQUIRE(licenses.Fix == HealthFix::AcceptLicenses);
    REQUIRE(OverallHealth(items) == HealthStatus::Failed);
}

TEST_CASE("RunHealthChecks reports an unreadable license state as a warning", "[health-check]") {
    HealthCheckDeps deps = AllGoodDeps();
    deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::CheckFailed; };

    const auto items = Run(FullSdk(), ValidJdk(), deps);

    REQUIRE(Find(items, HealthCheckId::Licenses).Status == HealthStatus::Warning);
    REQUIRE(OverallHealth(items) == HealthStatus::Warning);
}

TEST_CASE("RunHealthChecks fails an outdated JDK with a configure fix", "[health-check]") {
    JdkInfo jdk = ValidJdk();
    jdk.VersionString = "openjdk version \"11.0.2\"";
    jdk.MajorVersion = 11;
    jdk.IsValid = false;

    const auto items = Run(FullSdk(), jdk, AllGoodDeps());

    const auto &java = Find(items, HealthCheckId::JdkRuntime);
    REQUIRE(java.Status == HealthStatus::Failed);
    REQUIRE(java.Fix == HealthFix::ConfigureJdk);
}

TEST_CASE("RunHealthChecks warns when no JDK is found", "[health-check]") {
    const auto items = Run(FullSdk(), JdkInfo{}, AllGoodDeps());

    const auto &java = Find(items, HealthCheckId::JdkRuntime);
    REQUIRE(java.Status == HealthStatus::Warning);
    REQUIRE(java.Fix == HealthFix::ConfigureJdk);
}

TEST_CASE("RunHealthChecks flags missing cmdline-tools and skips checks that need them", "[health-check]") {
    SdkInfo sdk = FullSdk();
    sdk.AvdManagerPath.clear();
    sdk.SdkManagerPath.clear();

    const auto items = Run(sdk, ValidJdk(), AllGoodDeps());

    const auto &tools = Find(items, HealthCheckId::CmdlineTools);
    REQUIRE(tools.Status == HealthStatus::Failed);
    REQUIRE(tools.Fix == HealthFix::InstallCmdlineTools);

    REQUIRE(Find(items, HealthCheckId::ToolsRun).Status == HealthStatus::Skipped);
    REQUIRE(Find(items, HealthCheckId::Licenses).Status == HealthStatus::Skipped);
}

TEST_CASE("RunHealthChecks treats a deleted sdkmanager as missing tools, not a JDK problem", "[health-check]") {
    SdkInfo sdk = FullSdk();
    HealthCheckDeps deps = AllGoodDeps();
    deps.PathExists = [](const std::string &path) { return path.find("sdkmanager") == std::string::npos; };
    deps.SdkManagerVersion = [](const SdkInfo &) { return std::string{}; };

    const auto items = Run(sdk, ValidJdk(), deps);

    const auto &tools = Find(items, HealthCheckId::CmdlineTools);
    REQUIRE(tools.Status == HealthStatus::Failed);
    REQUIRE(tools.Fix == HealthFix::InstallCmdlineTools);
    REQUIRE(Find(items, HealthCheckId::ToolsRun).Fix == HealthFix::InstallCmdlineTools);
    REQUIRE(Find(items, HealthCheckId::JdkRuntime).Status == HealthStatus::Passed);
}

TEST_CASE("RunHealthChecks flags a missing apkanalyzer", "[health-check]") {
    SdkInfo sdk = FullSdk();
    sdk.ApkAnalyzerPath.clear();

    const auto items = Run(sdk, ValidJdk(), AllGoodDeps());

    const auto &tools = Find(items, HealthCheckId::CmdlineTools);
    REQUIRE(tools.Status == HealthStatus::Failed);
    REQUIRE(tools.Fix == HealthFix::InstallCmdlineTools);
}

TEST_CASE("RunHealthChecks warns about missing system images and low disk space", "[health-check]") {
    HealthCheckDeps deps = AllGoodDeps();
    deps.CountSystemImages = [](const SdkInfo &) { return static_cast<std::size_t>(0); };
    deps.FreeDiskSpace = [](const std::string &) {
        return std::optional<std::uint64_t>(512ULL * 1024ULL * 1024ULL);
    };

    const auto items = Run(FullSdk(), ValidJdk(), deps);

    const auto &images = Find(items, HealthCheckId::SystemImages);
    REQUIRE(images.Status == HealthStatus::Warning);
    REQUIRE(images.Fix == HealthFix::InstallSystemImage);

    REQUIRE(Find(items, HealthCheckId::DiskSpace).Status == HealthStatus::Warning);
    REQUIRE(OverallHealth(items) == HealthStatus::Warning);
}

TEST_CASE("RunHealthChecks fails the tools-run check when sdkmanager cannot start", "[health-check]") {
    HealthCheckDeps deps = AllGoodDeps();
    deps.SdkManagerVersion = [](const SdkInfo &) {
        return std::string("Exception in thread \"main\" java.lang.UnsupportedClassVersionError\n");
    };

    const auto items = Run(FullSdk(), ValidJdk(), deps);

    const auto &run = Find(items, HealthCheckId::ToolsRun);
    REQUIRE(run.Status == HealthStatus::Failed);
    REQUIRE(run.Fix == HealthFix::ConfigureJdk);
}

TEST_CASE("InterpretSdkManagerVersionOutput accepts version output with warnings", "[health-check]") {
    REQUIRE(InterpretSdkManagerVersionOutput("16.0\n") == "16.0");
    REQUIRE(
        InterpretSdkManagerVersionOutput("Warning: The SDK Manager CLI tool is deprecated.\n16.0\n") == "16.0"
    );
    REQUIRE_FALSE(InterpretSdkManagerVersionOutput("Exception in thread \"main\"\n").has_value());
    REQUIRE_FALSE(InterpretSdkManagerVersionOutput("").has_value());
}

TEST_CASE("InterpretSdkManagerVersionOutput reads the current cmdline-tools version line", "[health-check]") {
    const std::string output =
        "WARNING: The SDK Manager CLI tool (sdkmanager) is deprecated. Android CLI will be used instead.\n"
        "The 'android' binary can also be found in the cmdline-tools directory, and 'android sdk' is the replacement for 'sdkmanager'.\n"
        "To learn more about the Android CLI and how to use it, see the documentation (https://d.android.com/tools/agents/android-cli)\n"
        "\n"
        "1.0.16261425 (Android CLI)\n";

    REQUIRE(InterpretSdkManagerVersionOutput(output) == "1.0.16261425");
    REQUIRE(InterpretSdkManagerVersionOutput("unknown (Android CLI)\n") == "Android CLI");
}

TEST_CASE("RunHealthChecks accepts current cmdline-tools version output", "[health-check]") {
    HealthCheckDeps deps = AllGoodDeps();
    deps.SdkManagerVersion = [](const SdkInfo &) {
        return std::string(
            "WARNING: The SDK Manager CLI tool (sdkmanager) is deprecated. Android CLI will be used instead.\n"
            "1.0.16261425 (Android CLI)\n"
        );
    };

    const auto items = Run(FullSdk(), ValidJdk(), deps);

    const auto &run = Find(items, HealthCheckId::ToolsRun);
    REQUIRE(run.Status == HealthStatus::Passed);
    REQUIRE(run.Detail == "sdkmanager 1.0.16261425");
    REQUIRE(run.Fix == HealthFix::None);
}

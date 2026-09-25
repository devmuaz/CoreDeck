#include <catch2/catch_test_macros.hpp>

#include "core/sdk_manager.h"

using namespace CoreDeck;

TEST_CASE("InterpretSdkLicenseOutput accepts the cmdline-tools 23 license warning", "[sdk-manager]") {
    const std::string output =
        "WARNING: The SDK Manager CLI tool (sdkmanager) is deprecated. Android CLI will be used instead.\n"
        "Warning: The --licenses option is no longer needed.\n";

    REQUIRE(InterpretSdkLicenseOutput(output) == LicenseStatus::AllAccepted);
}

TEST_CASE("InterpretSdkLicenseOutput keeps the legacy sdkmanager results", "[sdk-manager]") {
    REQUIRE(InterpretSdkLicenseOutput("All SDK package licenses accepted.\n") == LicenseStatus::AllAccepted);
    REQUIRE(
        InterpretSdkLicenseOutput("2 licenses not accepted\nReview licenses that have not been accepted (y/N)?\n") ==
        LicenseStatus::SomeUnaccepted
    );
    REQUIRE(InterpretSdkLicenseOutput("Error: Could not determine SDK root.\n") == LicenseStatus::CheckFailed);
    REQUIRE(InterpretSdkLicenseOutput("") == LicenseStatus::CheckFailed);
}

TEST_CASE("ParseSdkManagerProgressLine reads legacy sdkmanager progress", "[sdk-manager]") {
    const auto progress = ParseSdkManagerProgressLine(
        "[=======================================] 100% Fetch remote repository..."
    );

    REQUIRE(progress.HasPercent);
    REQUIRE(progress.Percent == 100);
    REQUIRE(progress.Status == "Fetch remote repository...");
}

TEST_CASE("ParseSdkManagerProgressLine reads Android CLI download progress", "[sdk-manager]") {
    const auto progress = ParseSdkManagerProgressLine(
        "Downloading sys-img.zip ################ 7% (40.0 MB/560.0 MB) ETA: 90s"
    );

    REQUIRE(progress.HasPercent);
    REQUIRE(progress.Percent == 7);
    REQUIRE(progress.Status == "Downloading 40.0 MB / 560.0 MB");
}

TEST_CASE("ParseSdkManagerProgressLine reads Android CLI unzip progress", "[sdk-manager]") {
    const auto plain = ParseSdkManagerProgressLine(
        "Unzipping system-images/android-30/google_apis/arm64-v8a #### 40% (2/5) ETA: 3s"
    );
    const auto markedUp = ParseSdkManagerProgressLine(
        "Unzipping archive [@|green ####|@....] 15% (1.0 MB/8.0 MB) ETA: 1s"
    );

    REQUIRE(plain.HasPercent);
    REQUIRE(plain.Percent == 40);
    REQUIRE(plain.Status == "Unzipping 2 / 5");
    REQUIRE(markedUp.HasPercent);
    REQUIRE(markedUp.Percent == 15);
    REQUIRE(markedUp.Status == "Unzipping 1.0 MB / 8.0 MB");
}

TEST_CASE("ParseSdkManagerProgressLine ignores lines without a progress percent", "[sdk-manager]") {
    const auto warning = ParseSdkManagerProgressLine("Warning: package.xml parsing problem. Ignoring.");
    const auto empty = ParseSdkManagerProgressLine("");

    REQUIRE_FALSE(warning.HasPercent);
    REQUIRE(warning.Status.empty());
    REQUIRE_FALSE(empty.HasPercent);
}

TEST_CASE("SdkManagerInstallEnvironment forces a wide COLUMNS", "[sdk-manager]") {
    EnvVars env = {{.Name = "JAVA_HOME", .Value = "/jbr"}, {.Name = "COLUMNS", .Value = "80"}};
    const EnvVars installEnv = SdkManagerInstallEnvironment(std::move(env));

    REQUIRE(installEnv.size() == 2);
    REQUIRE(installEnv[1].Name == "COLUMNS");
    REQUIRE(installEnv[1].Value == "16384");

    const EnvVars added = SdkManagerInstallEnvironment({});
    REQUIRE(added.size() == 1);
    REQUIRE(added[0].Name == "COLUMNS");
    REQUIRE(added[0].Value == "16384");
}

TEST_CASE("RunSdkManagerInstall refuses to run without a sdkmanager binary", "[sdk-manager]") {
    const SdkInfo sdk; // No SdkManagerPath.
    REQUIRE_FALSE(RunSdkManagerInstall(sdk, {"--install", "platform-tools"}, "", nullptr));
}

TEST_CASE("RunSdkManager refuses to run without a sdkmanager binary", "[sdk-manager]") {
    const SdkInfo sdk; // No SdkManagerPath.
    REQUIRE_FALSE(RunSdkManager(sdk, {"--version"}).has_value());
}

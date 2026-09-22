#include <catch2/catch_test_macros.hpp>

#include "core/system_image.h"

using namespace CoreDeck;

TEST_CASE("ParseRemoteSystemImageLine reads legacy sdkmanager rows", "[system-image]") {
    const auto img = ParseRemoteSystemImageLine(
        "  system-images;android-35;google_apis;x86_64 | 9 | Google APIs Intel x86_64 Atom System Image"
    );

    REQUIRE(img.has_value());
    REQUIRE(img->PackagePath == "system-images;android-35;google_apis;x86_64");
    REQUIRE(img->ApiLevel == "35");
    REQUIRE(img->Variant == "google_apis");
    REQUIRE(img->Abi == "x86_64");
    REQUIRE_FALSE(img->IsInstalled);
}

TEST_CASE("ParseRemoteSystemImageLine reads Android CLI rows and keeps the legacy id", "[system-image]") {
    const auto img = ParseRemoteSystemImageLine(
        "  system-images/android-37.2-beta1/google_apis_playstore_ps16k/arm64-v8a    unknown    ->    1.0.0    16 KB Page Size Google Play ARM 64 v8a System Image"
    );

    REQUIRE(img.has_value());
    REQUIRE(img->PackagePath == "system-images;android-37.2-beta1;google_apis_playstore_ps16k;arm64-v8a");
    REQUIRE(img->ApiLevel == "37.2-beta1");
    REQUIRE(img->Variant == "google_apis_playstore_ps16k");
    REQUIRE(img->Abi == "arm64-v8a");
}

TEST_CASE("ParseAvdManagerDeviceList keeps device ids and drops avdmanager logs", "[system-image]") {
    const auto list = ParseAvdManagerDeviceList(
        "Error: Could not load devices from C:\\Users\\cheng\\AppData\\Local\\Android\\Sdk\\system-images\\android-37.2\\google_apis\\x86_64\\devices.xml\r\n"
        "Error: C:\\Users\\cheng\\AppData\\Local\\Android\\Sdk\\system-images\\android-37.2\\google_apis\\x86_64\\devices.xml (700000 B) is longer than limit (655360 B)\n"
        "pixel_7\n"
        "medium_phone\n"
        "Warning: Observed package id 'tools' in inconsistent location\n"
        "Nexus 5\n"
    );

    REQUIRE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.size() == 3);
    REQUIRE(list.Profiles[0].Id == "pixel_7");
    REQUIRE(list.Profiles[0].Name == "Pixel 7");
    REQUIRE(list.Profiles[1].Id == "medium_phone");
    REQUIRE(list.Profiles[1].Name == "Medium phone");
    REQUIRE(list.Profiles[2].Id == "Nexus 5");
    REQUIRE(list.Profiles[2].Name == "Nexus 5");
}

TEST_CASE("ParseAvdManagerDeviceList ignores unrelated diagnostics", "[system-image]") {
    const auto list = ParseAvdManagerDeviceList("Warning: package.xml parsing problem\npixel_7\n");

    REQUIRE_FALSE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.size() == 1);
    REQUIRE(list.Profiles[0].Id == "pixel_7");
}

TEST_CASE("ParseAvdManagerDeviceList leaves an empty list when only device-load errors are present", "[system-image]") {
    const auto list = ParseAvdManagerDeviceList("Error: Could not load devices from /sdk/devices.xml\n");

    REQUIRE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.empty());
}

TEST_CASE("InterpretSdkLicenseOutput accepts the cmdline-tools 23 license warning", "[system-image]") {
    const std::string output =
        "WARNING: The SDK Manager CLI tool (sdkmanager) is deprecated. Android CLI will be used instead.\n"
        "Warning: The --licenses option is no longer needed.\n";

    REQUIRE(InterpretSdkLicenseOutput(output) == LicenseStatus::AllAccepted);
}

TEST_CASE("InterpretSdkLicenseOutput keeps the legacy sdkmanager results", "[system-image]") {
    REQUIRE(InterpretSdkLicenseOutput("All SDK package licenses accepted.\n") == LicenseStatus::AllAccepted);
    REQUIRE(
        InterpretSdkLicenseOutput("2 licenses not accepted\nReview licenses that have not been accepted (y/N)?\n") ==
        LicenseStatus::SomeUnaccepted
    );
    REQUIRE(InterpretSdkLicenseOutput("Error: Could not determine SDK root.\n") == LicenseStatus::CheckFailed);
    REQUIRE(InterpretSdkLicenseOutput("") == LicenseStatus::CheckFailed);
}

TEST_CASE("ParseSdkManagerProgressLine reads legacy sdkmanager progress", "[system-image]") {
    const auto progress = ParseSdkManagerProgressLine(
        "[=======================================] 100% Fetch remote repository..."
    );

    REQUIRE(progress.HasPercent);
    REQUIRE(progress.Percent == 100);
    REQUIRE(progress.Status == "Fetch remote repository...");
}

TEST_CASE("ParseSdkManagerProgressLine reads Android CLI download progress", "[system-image]") {
    const auto progress = ParseSdkManagerProgressLine(
        "Downloading sys-img.zip ################ 7% (40.0 MB/560.0 MB) ETA: 90s"
    );

    REQUIRE(progress.HasPercent);
    REQUIRE(progress.Percent == 7);
    REQUIRE(progress.Status == "Downloading 40.0 MB / 560.0 MB");
}

TEST_CASE("ParseSdkManagerProgressLine reads Android CLI unzip progress", "[system-image]") {
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

TEST_CASE("ParseSdkManagerProgressLine ignores lines without a progress percent", "[system-image]") {
    const auto warning = ParseSdkManagerProgressLine("Warning: package.xml parsing problem. Ignoring.");
    const auto empty = ParseSdkManagerProgressLine("");

    REQUIRE_FALSE(warning.HasPercent);
    REQUIRE(warning.Status.empty());
    REQUIRE_FALSE(empty.HasPercent);
}

TEST_CASE("SdkManagerInstallEnvironment forces a wide COLUMNS", "[system-image]") {
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

TEST_CASE("ParseRemoteSystemImageLine ignores other packages", "[system-image]") {
    REQUIRE_FALSE(ParseRemoteSystemImageLine("  platforms/android-35    2.0.0    Android SDK Platform 35").has_value());
    REQUIRE_FALSE(ParseRemoteSystemImageLine("  build-tools;34.0.0 | 34.0.0 | Android SDK Build-Tools 34").has_value());
    REQUIRE_FALSE(ParseRemoteSystemImageLine("").has_value());
}

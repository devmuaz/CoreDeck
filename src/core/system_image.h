//
// Created by AbdulMuaz Aqeel on 19/04/2026.
//

#ifndef COREDECK_SYSTEM_IMAGE_H
#define COREDECK_SYSTEM_IMAGE_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "sdk.h"

namespace CoreDeck {
    struct DeviceProfile {
        std::string Id;
        std::string Name;
    };

    struct DeviceProfileList {
        std::vector<DeviceProfile> Profiles;
        bool SkippedDeviceDefinitions = false;
    };

    struct SystemImage {
        std::string ApiLevel;
        std::string Variant;
        std::string Abi;
        std::string PackagePath;
        std::string DisplayName;
    };

    struct RemoteSystemImage {
        std::string PackagePath;
        std::string ApiLevel;
        std::string Variant;
        std::string Abi;
        std::string DisplayName;
        bool IsInstalled = false;
    };

    struct InstallProgressData {
        std::mutex Mutex;
        float Percent = 0.0F;
        std::string StatusText;
        std::string DetailText;
        bool Finished = false;
        bool Succeeded = false;
    };

    DeviceProfileList ParseAvdManagerDeviceList(const std::string &output);

    DeviceProfileList ListDeviceProfiles(const SdkInfo &sdk);

    std::vector<SystemImage> ListSystemImages(const SdkInfo &sdk);

    // Reads one sdkmanager --list row. Accepts the legacy id
    // (system-images;android-35;google_apis;x86_64) and the Android CLI id
    // (system-images/android-35/google_apis/x86_64). The returned PackagePath is
    // always the legacy semicolon form, which avdmanager -k and older sdkmanager
    // require. Current sdkmanager accepts that form and rewrites it internally.
    std::optional<RemoteSystemImage> ParseRemoteSystemImageLine(const std::string &line);

    std::vector<RemoteSystemImage> ListRemoteSystemImages(
        const SdkInfo &sdk,
        const std::vector<SystemImage> &installedImages
    );

    // Legacy sdkmanager: "[====] 42% Fetch remote repository..."
    // cmdline-tools 23: "message #### 42% (1.2 MB/80.0 MB) ETA: 12s"
    struct SdkManagerProgressLine {
        bool HasPercent = false;
        int Percent = 0;
        std::string Status;
    };

    SdkManagerProgressLine ParseSdkManagerProgressLine(const std::string &line);

    // The Android CLI redraws progress with a carriage return. Java only
    // flushes that stream once a line is wider than its stdout buffer, and it
    // reads COLUMNS before asking the terminal how wide it is.
    EnvVars SdkManagerInstallEnvironment(EnvVars env);

    bool InstallSystemImage(
        const SdkInfo &sdk,
        const std::string &packagePath,
        const std::shared_ptr<InstallProgressData> &progress = nullptr
    );

    bool UninstallSystemImage(const SdkInfo &sdk, const std::string &packagePath);

    enum class LicenseStatus : uint8_t {
        AllAccepted,
        SomeUnaccepted,
        CheckFailed,
    };

    LicenseStatus InterpretSdkLicenseOutput(const std::string &output);

    LicenseStatus CheckSdkLicenses(const SdkInfo &sdk);

    bool AcceptSdkLicenses(const SdkInfo &sdk);
}

#endif // COREDECK_SYSTEM_IMAGE_H

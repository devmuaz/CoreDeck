//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#ifndef COREDECK_SDK_MANAGER_H
#define COREDECK_SDK_MANAGER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "sdk.h"

namespace CoreDeck {
    // Reads one sdkmanager output line. Understands both progress formats:
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

    using SdkManagerProgressSink =
        std::function<void(const SdkManagerProgressLine &parsed, const std::string &raw)>;

    // Streams "sdkmanager <args>" with the progress-friendly environment and
    // feeds every output line (parsed and raw) to the sink. Returns false when
    // the sdkmanager binary is not available.
    bool RunSdkManagerInstall(
        const SdkInfo &sdk,
        const std::vector<std::string> &args,
        const std::string &stdinInput,
        const SdkManagerProgressSink &onLine
    );

    enum class LicenseStatus : uint8_t {
        AllAccepted,
        SomeUnaccepted,
        CheckFailed,
    };

    LicenseStatus InterpretSdkLicenseOutput(const std::string &output);

    LicenseStatus CheckSdkLicenses(const SdkInfo &sdk);

    bool AcceptSdkLicenses(const SdkInfo &sdk);
}

#endif // COREDECK_SDK_MANAGER_H

//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef COREDECK_SDK_H
#define COREDECK_SDK_H
#include <string>

#include "env.h"

namespace CoreDeck {
    struct SdkInfo {
        std::string SdkPath;
        std::string EmulatorPath;
        std::string AvdManagerPath;
        std::string SdkManagerPath;
        EnvVars ToolEnv;
        bool IsFound = false;
    };

    SdkInfo DetectAndroidSdk();

    // Probes a specific directory without consulting the override or environment.
    // Tool paths are filled in when present, so a partially bootstrapped SDK
    // (cmdline-tools but no emulator yet) still reports its SdkManagerPath.
    SdkInfo ProbeAndroidSdk(const std::string &sdkPath);
}

#endif // COREDECK_SDK_H

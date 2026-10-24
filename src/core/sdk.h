//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef COREDECK_SDK_H
#define COREDECK_SDK_H
#include <string>

#include "env.h"

namespace CoreDeck::Core {
    struct SdkInfo {
        std::string SdkPath;
        std::string EmulatorPath;
        std::string AvdManagerPath;
        std::string SdkManagerPath;
        std::string ApkAnalyzerPath;
        std::string Aapt2Path;
        std::string ZipalignPath;
        std::string ApksignerPath;
        EnvVars ToolEnv;
        bool IsFound = false;
    };

    SdkInfo DetectAndroidSdk();

    SdkInfo ProbeAndroidSdk(const std::string &sdkPath);

    std::string FindInstalledBuildTool(const std::string &sdkPath, const std::string &name);

    std::string FindInstalledAapt2(const std::string &sdkPath);

    void RefreshAndroidSdk(SdkInfo &sdk);
}

#endif // COREDECK_SDK_H

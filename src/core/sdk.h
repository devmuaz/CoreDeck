//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef COREDECK_SDK_H
#define COREDECK_SDK_H
#include <cstdint>
#include <string>
#include <string_view>

#include "env.h"

namespace CoreDeck::Core {

    constexpr const char *SDK_LICENSE_URL = "https://developer.android.com/studio/terms";
    constexpr const char *ANDROID_REPOSITORY_URL = "https://dl.google.com/android/repository/";
    constexpr const char *CMDLINE_TOOLS_VERSION = "15859902";
    constexpr const char *BOOTSTRAP_BUILD_TOOLS_PACKAGE = "build-tools;37.0.0";
    constexpr const char *SDK_MANAGER_PROGRESS_COLUMNS = "16384";
    constexpr std::uint64_t BOOTSTRAP_REQUIRED_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr std::uint64_t BOOTSTRAP_TOOLS_REQUIRED_BYTES = 512ULL * 1024ULL * 1024ULL;
    constexpr const char *ENV_ANDROID_HOME = "ANDROID_HOME";
    constexpr const char *ENV_ANDROID_SDK_ROOT = "ANDROID_SDK_ROOT";
    constexpr const char *ENV_COLUMNS = "COLUMNS";

    struct SdkInfo {
        std::string SdkPath;
        std::string EmulatorPath;
        std::string AvdManagerPath;
        std::string SdkManagerPath;
        std::string ApkAnalyzerPath;
        std::string Aapt2Path;
        std::string ZipalignPath;
        std::string ApksignerPath;
        Platform::EnvVars ToolEnv;
        bool IsFound = false;
    };

    SdkInfo DetectAndroidSdk(std::string_view savedPath = {});

    std::string GetAndroidSdkDefaultPath();

    bool ValidateSdkPath(const std::string &path);

    SdkInfo ProbeAndroidSdk(const std::string &sdkPath);

    std::string FindInstalledBuildTool(const std::string &sdkPath, const std::string &name);

    std::string FindInstalledAapt2(const std::string &sdkPath);

    void RefreshAndroidSdk(SdkInfo &sdk);
}

#endif // COREDECK_SDK_H

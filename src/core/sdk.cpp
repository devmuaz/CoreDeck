//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include <filesystem>

#include "sdk.h"
#include "paths.h"

namespace CoreDeck {
    SdkInfo DetectAndroidSdk() {
        SdkInfo sdk;

        const std::string savedPath = Paths::Onboarding::LoadSdkPathOverride();
        if (!savedPath.empty() && std::filesystem::exists(savedPath)) {
            sdk.SdkPath = savedPath;
        } else {
            const char *sdkEnv = std::getenv("ANDROID_HOME");
            if (!sdkEnv) sdkEnv = std::getenv("ANDROID_SDK_ROOT");

            if (sdkEnv) {
                sdk.SdkPath = sdkEnv;
            } else {
                const std::string defaultPath = Paths::GetAndroidSdkDefaultPath();
                if (std::filesystem::exists(defaultPath)) sdk.SdkPath = defaultPath;
            }
        }

        if (sdk.SdkPath.empty()) return sdk;

        sdk.EmulatorPath = Paths::JoinPaths({sdk.SdkPath, "emulator", "emulator" + Paths::GetExecutableExtension()});

        const std::string cmdlineLatest = Paths::JoinPaths({
            sdk.SdkPath, "cmdline-tools", "latest", "bin", "avdmanager" + Paths::GetExecutableExtension()
        });

        if (std::filesystem::exists(cmdlineLatest)) {
            sdk.AvdManagerPath = cmdlineLatest;
        } else {
            const std::string cmdlineRoot = Paths::JoinPaths({sdk.SdkPath, "cmdline-tools"});
            if (std::filesystem::exists(cmdlineRoot) && std::filesystem::is_directory(cmdlineRoot)) {
                for (const auto &entry: std::filesystem::directory_iterator(cmdlineRoot)) {
                    if (!entry.is_directory()) continue;
                    const std::string candidate = Paths::JoinPaths({
                        entry.path().string(), "bin", "avdmanager" + Paths::GetExecutableExtension()
                    });
                    if (std::filesystem::exists(candidate)) {
                        sdk.AvdManagerPath = candidate;
                        break;
                    }
                }
            }
        }

        if (!sdk.AvdManagerPath.empty()) {
            std::filesystem::path avdMgrPath(sdk.AvdManagerPath);
            const std::string sdkMgrCandidate = Paths::JoinPaths({
                avdMgrPath.parent_path().string(), "sdkmanager" + Paths::GetExecutableExtension()
            });
            if (std::filesystem::exists(sdkMgrCandidate)) {
                sdk.SdkManagerPath = sdkMgrCandidate;
            }
        }

        if (std::filesystem::exists(sdk.EmulatorPath)) sdk.IsFound = true;
        return sdk;
    }
}

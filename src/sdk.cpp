//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "sdk.h"
#include "paths.h"
#include <filesystem>
#include <sstream>

#include "process.h"

namespace CoreDeck {
    SdkInfo DetectAndroidSdk() {
        SdkInfo sdk;

        const char *sdkEnv = std::getenv("ANDROID_HOME");
        if (!sdkEnv) {
            sdkEnv = std::getenv("ANDROID_SDK_ROOT");
        }

        if (!sdkEnv) {
            const std::string defaultPath = Paths::GetAndroidSdkDefaultPath();
            if (std::filesystem::exists(defaultPath)) sdk.SdkPath = defaultPath;
        } else {
            sdk.SdkPath = sdkEnv;
        }

        if (sdk.SdkPath.empty()) return sdk;

        sdk.EmulatorPath = Paths::JoinPaths({sdk.SdkPath, "emulator", "emulator" + Paths::GetExecutableExtension()});

        // Detect avdmanager in cmdline-tools (latest or versioned)
        const std::string cmdlineLatest = Paths::JoinPaths({
            sdk.SdkPath, "cmdline-tools", "latest", "bin", "avdmanager" + Paths::GetExecutableExtension()
        });
        if (std::filesystem::exists(cmdlineLatest)) {
            sdk.AvdManagerPath = cmdlineLatest;
        } else {
            // Search versioned cmdline-tools directories
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

        if (std::filesystem::exists(sdk.EmulatorPath)) sdk.IsFound = true;
        return sdk;
    }

    std::vector<std::string> ListAvailableAvds(const SdkInfo &sdk) {
        std::vector<std::string> avds;

        if (!sdk.IsFound) {
            return avds;
        }

        const std::string output = RunCommand("\"" + sdk.EmulatorPath + "\" -list-avds");
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                avds.emplace_back(line);
            }
        }

        return avds;
    }

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName) {
        if (!sdk.AvdManagerPath.empty()) {
            const std::string cmd = "\"" + sdk.AvdManagerPath + "\" delete avd -n " + avdName;
            RunCommand(cmd);
        } else {
            // Fallback: manually delete the AVD files
            const std::string avdDir = Paths::GetAvdDirectory();
            if (avdDir.empty()) return false;

            const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
            const std::string avdIni = Paths::JoinPaths({avdDir, avdName + ".ini"});

            std::error_code ec;
            std::filesystem::remove_all(avdFolder, ec);
            if (ec) return false;

            std::filesystem::remove(avdIni, ec);
        }

        // Verify the AVD no longer exists
        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
        return !std::filesystem::exists(avdFolder);
    }
}

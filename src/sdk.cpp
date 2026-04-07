//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "sdk.h"
#include <filesystem>
#include <sstream>

#include "process.h"

namespace Emu {
    SdkInfo DetectAndroidSdk() {
        SdkInfo sdk;

        const char *sdkEnv = std::getenv("ANDROID_HOME");
        if (!sdkEnv) {
            sdkEnv = std::getenv("ANDROID_SDK_ROOT");
        }

        if (!sdkEnv) {
            const std::string defaultPath = std::string(std::getenv("HOME")) + "/Library/Android/sdk";

            if (std::filesystem::exists(defaultPath)) {
                sdk.SdkPath = defaultPath;
            }
        } else {
            sdk.SdkPath = sdkEnv;
        }

        if (sdk.SdkPath.empty()) {
            return sdk;
        }

        sdk.EmulatorPath = sdk.SdkPath + "/emulator/emulator";

        if (std::filesystem::exists(sdk.EmulatorPath)) {
            sdk.IsFound = true;
        }

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
}

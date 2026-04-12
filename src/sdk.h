//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef EMU_LAUNCHER_SDK_H
#define EMU_LAUNCHER_SDK_H
#include <string>
#include <vector>

namespace CoreDeck {
    struct SdkInfo {
        std::string SdkPath;
        std::string EmulatorPath;
        std::string AvdManagerPath;
        bool IsFound = false;
    };

    SdkInfo DetectAndroidSdk();

    std::vector<std::string> ListAvailableAvds(const SdkInfo &sdk);

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName);
}

#endif //EMU_LAUNCHER_SDK_H

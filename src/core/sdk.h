//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef EMU_LAUNCHER_SDK_H
#define EMU_LAUNCHER_SDK_H
#include <string>

namespace CoreDeck {
    struct SdkInfo {
        std::string SdkPath;
        std::string EmulatorPath;
        std::string AvdManagerPath;
        std::string SdkManagerPath;
        bool IsFound = false;
    };

    SdkInfo DetectAndroidSdk();
}

#endif //EMU_LAUNCHER_SDK_H

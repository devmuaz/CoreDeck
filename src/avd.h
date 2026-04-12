//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#ifndef EMU_LAUNCHER_AVD_INFO_H
#define EMU_LAUNCHER_AVD_INFO_H

#include <string>
#include <vector>

namespace CoreDeck {
    struct AvdInfo {
        std::string Name;
        std::string DisplayName;
        std::string Device;
        std::string ApiLevel;
        std::string Abi;
        std::string SdCard;
        std::string RamSize;
        std::string ScreenResolution;
        std::string GpuMode;
        std::string Arch;
        std::string Path;
    };

    std::vector<AvdInfo> LoadAvds(const std::vector<std::string> &avdNames);

    static AvdInfo LoadAvd(const std::string &avdName);
}

#endif //EMU_LAUNCHER_AVD_INFO_H

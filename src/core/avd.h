//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#ifndef EMU_LAUNCHER_AVD_INFO_H
#define EMU_LAUNCHER_AVD_INFO_H

#include <string>
#include <vector>

#include "sdk.h"

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

    struct SystemImage {
        std::string ApiLevel;
        std::string Variant;
        std::string Abi;
        std::string PackagePath;
        std::string DisplayName;
    };

    struct DeviceProfile {
        std::string Id;
        std::string Name;
    };

    struct CreateAvdParams {
        std::string Name;
        std::string DisplayName;
        std::string SystemImagePackagePath;
        std::string DeviceId;
        std::string RamSize;
        std::string SdCardSize;
        std::string GpuMode;
    };

    std::vector<AvdInfo> LoadAvds(const std::vector<std::string> &avdNames);

    std::vector<std::string> ListAvdNames(const SdkInfo &sdk);

    std::vector<SystemImage> ListSystemImages(const SdkInfo &sdk);

    std::vector<DeviceProfile> ListDeviceProfiles(const SdkInfo &sdk);

    bool CreateAvd(const SdkInfo &sdk, const CreateAvdParams &params);

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName);
}

#endif //EMU_LAUNCHER_AVD_INFO_H

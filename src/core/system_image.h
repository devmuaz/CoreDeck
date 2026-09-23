//
// Created by AbdulMuaz Aqeel on 19/04/2026.
//

#ifndef COREDECK_SYSTEM_IMAGE_H
#define COREDECK_SYSTEM_IMAGE_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "sdk.h"

namespace CoreDeck {
    struct SystemImage {
        std::string ApiLevel;
        std::string Variant;
        std::string Abi;
        std::string PackagePath;
        std::string DisplayName;
    };

    struct RemoteSystemImage {
        std::string PackagePath;
        std::string ApiLevel;
        std::string Variant;
        std::string Abi;
        std::string DisplayName;
        bool IsInstalled = false;
    };

    struct InstallProgressData {
        std::mutex Mutex;
        float Percent = 0.0F;
        std::string StatusText;
        std::string DetailText;
        bool Finished = false;
        bool Succeeded = false;
    };

    std::vector<SystemImage> ListSystemImages(const SdkInfo &sdk);

    // Reads one sdkmanager --list row. Accepts the legacy id
    // (system-images;android-35;google_apis;x86_64) and the Android CLI id
    // (system-images/android-35/google_apis/x86_64). The returned PackagePath is
    // always the legacy semicolon form, which avdmanager -k and older sdkmanager
    // require. Current sdkmanager accepts that form and rewrites it internally.
    std::optional<RemoteSystemImage> ParseRemoteSystemImageLine(const std::string &line);

    std::vector<RemoteSystemImage> ListRemoteSystemImages(
        const SdkInfo &sdk,
        const std::vector<SystemImage> &installedImages
    );

    bool InstallSystemImage(
        const SdkInfo &sdk,
        const std::string &packagePath,
        const std::shared_ptr<InstallProgressData> &progress = nullptr
    );

    bool UninstallSystemImage(const SdkInfo &sdk, const std::string &packagePath);
}

#endif // COREDECK_SYSTEM_IMAGE_H

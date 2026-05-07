//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#ifndef COREDECK_SKIN_H
#define COREDECK_SKIN_H

#include <optional>
#include <string>
#include <vector>

#include "sdk.h"

namespace CoreDeck {
    enum class SkinSource {
        Sdk,
        SystemImage,
        Platform,
    };

    struct Skin {
        std::string Name;
        std::string DisplayName;
        std::string Path;
        SkinSource Source = SkinSource::Sdk;
    };

    std::vector<Skin> ListSkins(const SdkInfo &sdk);

    std::optional<Skin> FindSkinForDevice(const std::vector<Skin> &skins, const std::string &deviceId);

    const char *SkinSourceLabel(const SkinSource &source);
}

#endif // COREDECK_SKIN_H
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>

#include "core/skin.h"

namespace fs = std::filesystem;
using namespace CoreDeck;

namespace {
    fs::path UniqueTempDir(const std::string &prefix) {
        std::random_device rd;
        const fs::path base = fs::temp_directory_path() / (prefix + "_" + std::to_string(rd()));
        fs::create_directories(base);
        return base;
    }

    void MakeSkin(const fs::path &dir) {
        fs::create_directories(dir);
        std::ofstream(dir / "layout") << "parts {}\n";
    }
}

TEST_CASE("ListSkins discovers skins from $SDK/skins", "[skin][list]") {
    const fs::path sdk = UniqueTempDir("coredeck_skin_sdk");
    MakeSkin(sdk / "skins" / "pixel_7");
    MakeSkin(sdk / "skins" / "WearOSRound");

    SdkInfo info;
    info.SdkPath = sdk.string();

    const auto skins = ListSkins(info);
    REQUIRE(skins.size() == 2);

    bool sawPixel = false, sawWear = false;
    for (const auto &s: skins) {
        if (s.Name == "pixel_7") {
            sawPixel = true;
            REQUIRE(s.Source == SkinSource::Sdk);
        }
        if (s.Name == "WearOSRound") sawWear = true;
    }
    REQUIRE(sawPixel);
    REQUIRE(sawWear);

    fs::remove_all(sdk);
}

TEST_CASE("ListSkins prefers SDK-level skin over system-image-bundled duplicate", "[skin][dedup]") {
    const fs::path sdk = UniqueTempDir("coredeck_skin_dedup");
    MakeSkin(sdk / "skins" / "pixel_7");
    MakeSkin(sdk / "system-images" / "android-34" / "google_apis" / "arm64-v8a" / "skins" / "pixel_7");

    SdkInfo info;
    info.SdkPath = sdk.string();

    const auto skins = ListSkins(info);
    REQUIRE(skins.size() == 1);
    REQUIRE(skins[0].Name == "pixel_7");
    REQUIRE(skins[0].Source == SkinSource::Sdk);

    fs::remove_all(sdk);
}

TEST_CASE("ListSkins ignores directories without a layout file", "[skin][list]") {
    const fs::path sdk = UniqueTempDir("coredeck_skin_invalid");
    fs::create_directories(sdk / "skins" / "not_a_skin");

    SdkInfo info;
    info.SdkPath = sdk.string();

    const auto skins = ListSkins(info);
    REQUIRE(skins.empty());

    fs::remove_all(sdk);
}

TEST_CASE("FindSkinForDevice returns exact-name match when present", "[skin][match]") {
    std::vector<Skin> skins = {
        {"pixel_6", "Pixel 6", "/p6", SkinSource::Sdk},
        {"pixel_7", "Pixel 7", "/p7", SkinSource::Sdk},
    };

    const auto match = FindSkinForDevice(skins, "pixel_7");
    REQUIRE(match.has_value());
    REQUIRE(match->Name == "pixel_7");
}

TEST_CASE("FindSkinForDevice returns nullopt for unknown device", "[skin][match]") {
    const std::vector<Skin> skins = {
        {"pixel_7", "Pixel 7", "/p7", SkinSource::Sdk},
    };

    const auto match = FindSkinForDevice(skins, "totally_unknown_device_xyz");
    REQUIRE_FALSE(match.has_value());
}

TEST_CASE("FindSkinForDevice handles empty inputs", "[skin][match]") {
    REQUIRE_FALSE(FindSkinForDevice({}, "pixel_7").has_value());

    const std::vector<Skin> skins = {
        {"pixel_7", "Pixel 7", "/p7", SkinSource::Sdk}
    };
    REQUIRE_FALSE(FindSkinForDevice(skins, "").has_value());
}
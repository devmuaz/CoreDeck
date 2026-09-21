#include <catch2/catch_test_macros.hpp>

#include "core/system_image.h"

using namespace CoreDeck;

TEST_CASE("ParseRemoteSystemImageLine reads legacy sdkmanager rows", "[system-image]") {
    const auto img = ParseRemoteSystemImageLine(
        "  system-images;android-35;google_apis;x86_64 | 9 | Google APIs Intel x86_64 Atom System Image"
    );

    REQUIRE(img.has_value());
    REQUIRE(img->PackagePath == "system-images;android-35;google_apis;x86_64");
    REQUIRE(img->ApiLevel == "35");
    REQUIRE(img->Variant == "google_apis");
    REQUIRE(img->Abi == "x86_64");
    REQUIRE_FALSE(img->IsInstalled);
}

TEST_CASE("ParseRemoteSystemImageLine reads Android CLI rows and keeps the legacy id", "[system-image]") {
    const auto img = ParseRemoteSystemImageLine(
        "  system-images/android-37.2-beta1/google_apis_playstore_ps16k/arm64-v8a    unknown    ->    1.0.0    16 KB Page Size Google Play ARM 64 v8a System Image"
    );

    REQUIRE(img.has_value());
    REQUIRE(img->PackagePath == "system-images;android-37.2-beta1;google_apis_playstore_ps16k;arm64-v8a");
    REQUIRE(img->ApiLevel == "37.2-beta1");
    REQUIRE(img->Variant == "google_apis_playstore_ps16k");
    REQUIRE(img->Abi == "arm64-v8a");
}

TEST_CASE("ParseRemoteSystemImageLine ignores other packages", "[system-image]") {
    REQUIRE_FALSE(ParseRemoteSystemImageLine("  platforms/android-35    2.0.0    Android SDK Platform 35").has_value());
    REQUIRE_FALSE(ParseRemoteSystemImageLine("  build-tools;34.0.0 | 34.0.0 | Android SDK Build-Tools 34").has_value());
    REQUIRE_FALSE(ParseRemoteSystemImageLine("").has_value());
}

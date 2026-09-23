#include <catch2/catch_test_macros.hpp>

#include "core/avd_manager.h"

using namespace CoreDeck;

TEST_CASE("ParseAvdManagerDeviceList keeps device ids and drops avdmanager logs", "[avd]") {
    const auto list = ParseAvdManagerDeviceList(
        "Error: Could not load devices from C:\\Users\\cheng\\AppData\\Local\\Android\\Sdk\\system-images\\android-37.2\\google_apis\\x86_64\\devices.xml\r\n"
        "Error: C:\\Users\\cheng\\AppData\\Local\\Android\\Sdk\\system-images\\android-37.2\\google_apis\\x86_64\\devices.xml (700000 B) is longer than limit (655360 B)\n"
        "pixel_7\n"
        "medium_phone\n"
        "Warning: Observed package id 'tools' in inconsistent location\n"
        "Nexus 5\n"
    );

    REQUIRE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.size() == 3);
    REQUIRE(list.Profiles[0].Id == "pixel_7");
    REQUIRE(list.Profiles[0].Name == "Pixel 7");
    REQUIRE(list.Profiles[1].Id == "medium_phone");
    REQUIRE(list.Profiles[1].Name == "Medium phone");
    REQUIRE(list.Profiles[2].Id == "Nexus 5");
    REQUIRE(list.Profiles[2].Name == "Nexus 5");
}

TEST_CASE("ParseAvdManagerDeviceList ignores unrelated diagnostics", "[avd]") {
    const auto list = ParseAvdManagerDeviceList("Warning: package.xml parsing problem\npixel_7\n");

    REQUIRE_FALSE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.size() == 1);
    REQUIRE(list.Profiles[0].Id == "pixel_7");
}

TEST_CASE("ParseAvdManagerDeviceList leaves an empty list when only device-load errors are present", "[avd]") {
    const auto list = ParseAvdManagerDeviceList("Error: Could not load devices from /sdk/devices.xml\n");

    REQUIRE(list.SkippedDeviceDefinitions);
    REQUIRE(list.Profiles.empty());
}

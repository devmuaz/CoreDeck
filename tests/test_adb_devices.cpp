#include <catch2/catch_test_macros.hpp>

#include "core/adb_devices.h"

using namespace CoreDeck::Core;

TEST_CASE("ParseAdbDevices reads metadata and emulator identity", "[adb]") {
    const auto devices = ParseAdbDevices(
        "List of devices attached\r\n"
        "emulator-5554\tdevice product:sdk_phone model:Pixel_9 device:emu64 transport_id:1\r\n"
        "R58M123\toffline usb:1-2 product:gts model:Galaxy_S device:gts transport_id:2\r\n"
    );

    REQUIRE(devices.size() == 2);
    REQUIRE(devices.at(0).Serial == "emulator-5554");
    REQUIRE(devices.at(0).State == AdbDeviceState::Device);
    REQUIRE(devices.at(0).Model == "Pixel_9");
    REQUIRE(devices.at(0).IsEmulator);
    REQUIRE(devices.at(0).EmulatorConsolePort == 5554);
    REQUIRE(devices.at(1).State == AdbDeviceState::Offline);
    REQUIRE_FALSE(devices.at(1).IsEmulator);
}

TEST_CASE("ParseAdbDevices preserves authorization and permission states", "[adb]") {
    const auto devices = ParseAdbDevices(
        "ABC\tunauthorized usb:1-1 transport_id:3\n"
        "DEF\tno permissions (user in plugdev group; are your udev rules wrong?)\n"
        "GHI\trecovery transport_id:4\n"
    );

    REQUIRE(devices.size() == 3);
    REQUIRE(devices.at(0).State == AdbDeviceState::Unauthorized);
    REQUIRE(devices.at(1).State == AdbDeviceState::NoPermissions);
    REQUIRE(devices.at(2).State == AdbDeviceState::Recovery);
}

TEST_CASE("ParseAdbDevices ignores daemon messages and empty snapshots", "[adb]") {
    REQUIRE(ParseAdbDevices("* daemon started successfully *\nList of devices attached\n\n").empty());
}

TEST_CASE("ParseAdbForwards reads forward and reverse mappings", "[adb]") {
    const auto forward = ParseAdbForwards("emulator-5554 tcp:8080 tcp:80\n");
    const auto reverse = ParseAdbForwards("host tcp:3000 tcp:4000\nhost-12 tcp:10100 tcp:1122\n", true);
    const auto bareReverse = ParseAdbForwards("tcp:1 tcp:2\n", true);
    const auto namedReverse = ParseAdbForwards("emulator-5554 tcp:9 tcp:8\n", true);

    REQUIRE(forward.size() == 1);
    REQUIRE(forward.front().Serial == "emulator-5554");
    REQUIRE(forward.front().Local == "tcp:8080");
    REQUIRE(forward.front().Remote == "tcp:80");
    REQUIRE_FALSE(forward.front().Reverse);

    REQUIRE(reverse.size() == 2);
    REQUIRE(reverse.front().Serial.empty());
    REQUIRE(reverse.front().Local == "tcp:3000");
    REQUIRE(reverse.front().Remote == "tcp:4000");
    REQUIRE(reverse.front().Reverse);
    REQUIRE(reverse.back().Serial.empty());
    REQUIRE(reverse.back().Local == "tcp:10100");
    REQUIRE(reverse.back().Remote == "tcp:1122");

    REQUIRE(bareReverse.size() == 1);
    REQUIRE(bareReverse.front().Local == "tcp:1");
    REQUIRE(bareReverse.front().Remote == "tcp:2");

    REQUIRE(namedReverse.size() == 1);
    REQUIRE(namedReverse.front().Serial == "emulator-5554");
    REQUIRE(namedReverse.front().Local == "tcp:9");
}

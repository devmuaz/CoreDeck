#include <catch2/catch_test_macros.hpp>

#include "core/adb_client.h"

using namespace CoreDeck::Core;

TEST_CASE("BuildAdbArguments targets a serial before the command", "[adb]") {
    const auto args = BuildAdbArguments("emulator-5554", {"shell", "getprop ro.product.model"});

    REQUIRE(args == std::vector<std::string>{"-s", "emulator-5554", "shell", "getprop ro.product.model"});
}

TEST_CASE("BuildAdbArguments leaves global commands untargeted", "[adb]") {
    REQUIRE(BuildAdbArguments({}, {"devices", "-l"}) == std::vector<std::string>{"devices", "-l"});
}

TEST_CASE("ParseAdbVersion extracts the platform-tools version", "[adb]") {
    const std::string output =
        "Android Debug Bridge version 1.0.41\n"
        "Version 36.0.0-13206524\n"
        "Installed as /sdk/platform-tools/adb\n";

    REQUIRE(ParseAdbVersion(output) == "1.0.41");
    REQUIRE(ParseAdbVersion("error: unavailable\n").empty());
}

TEST_CASE("Connect and disconnect reports follow adb's text, not only the exit code", "[adb]") {
    REQUIRE(AdbConnectReportSucceeded("connected to 192.168.68.105:5555\n"));
    REQUIRE(AdbConnectReportSucceeded("already connected to 192.168.68.105:5555\n"));
    REQUIRE_FALSE(AdbConnectReportSucceeded(
        "failed to connect to '192.168.68.105:5555': Connection refused\n"
    ));
    REQUIRE(AdbDisconnectReportSucceeded("disconnected 192.168.68.105:5555\n"));
    REQUIRE_FALSE(AdbDisconnectReportSucceeded("error: no such device '192.168.68.105:5555'\n"));
}

TEST_CASE("Wireless address parsing prefers the Wi-Fi interface", "[adb]") {
    const std::string interfaces =
        "1: lo: <LOOPBACK,UP,LOWER_UP>\n"
        "    inet 127.0.0.1/8 scope host lo\n"
        "12: rmnet0: <UP,LOWER_UP>\n"
        "    inet 10.42.0.5/32 scope global rmnet0\n"
        "34: wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP>\n"
        "    inet 192.168.68.105/24 brd 192.168.68.255 scope global wlan0\n";
    REQUIRE(ParseWirelessIpv4(interfaces) == std::optional<std::string>{"192.168.68.105"});

    const std::string route = "8.8.8.8 via 192.168.68.1 dev wlan0 src 192.168.68.105 uid 2000\n";
    REQUIRE(ParseWirelessIpv4(route) == std::optional<std::string>{"192.168.68.105"});
    REQUIRE(ParseWirelessIpv4("192.168.68.105\n") == std::optional<std::string>{"192.168.68.105"});
    REQUIRE_FALSE(ParseWirelessIpv4(interfaces.substr(0, interfaces.find("34:"))).has_value());
    REQUIRE_FALSE(ParseWirelessIpv4("").has_value());
}

TEST_CASE("Network endpoints can be read back from adb connect text", "[adb]") {
    REQUIRE(FindAdbNetworkEndpoint("connected to 192.168.68.105:5555\n") == std::optional<std::string>{"192.168.68.105:5555"});
    REQUIRE(
        FindAdbNetworkEndpoint("failed to connect to '192.168.68.105:5555': Connection refused\n") ==
        std::optional<std::string>{"192.168.68.105:5555"}
    );
    REQUIRE_FALSE(FindAdbNetworkEndpoint("error: device offline\n").has_value());
}

TEST_CASE("RunAdb reports a missing tool without starting a process", "[adb]") {
    const SdkInfo sdk;
    const AdbCommandResult result = RunAdb(sdk, {"version"});

    REQUIRE(result.Error == AdbError::ToolMissing);
    REQUIRE_FALSE(result.Started);
}

TEST_CASE("Package list parsing keeps names and drops command noise", "[adb]") {
    const std::string output =
        "package:com.example.zebra\r\n"
        "WARNING: linker: unused\n"
        "package:com.example.app\n"
        "package:/data/app/base.apk=com.example.game uid:10001\n"
        "package:com.example.app\n"
        "package:\n"
        "package:   \n";

    REQUIRE(ListAdbPackagesCommand() == std::vector<std::string>{"shell", "pm", "list", "packages"});
    REQUIRE(ListAdbPackagesCommand(AdbPackageListScope::All) == std::vector<std::string>{"shell", "pm", "list", "packages"});
    REQUIRE(ListAdbPackagesCommand(AdbPackageListScope::ThirdParty) == std::vector<std::string>{"shell", "pm", "list", "packages", "-3"});
    REQUIRE(ListAdbPackagesCommand(AdbPackageListScope::System) == std::vector<std::string>{"shell", "pm", "list", "packages", "-s"});
    REQUIRE(
        ParseAdbPackages(output) ==
        std::vector<std::string>{"com.example.app", "com.example.game", "com.example.zebra"}
    );
    REQUIRE(ParseAdbPackages("").empty());
}

TEST_CASE("Keep-data uninstall uses the package service", "[adb]") {
    REQUIRE(UninstallAdbCommand("com.example.app", false) == std::vector<std::string>{"uninstall", "com.example.app"});
    REQUIRE(
        UninstallAdbCommand("com.example.app", true) ==
        std::vector<std::string>{"shell", "cmd", "package", "uninstall", "-k", "com.example.app"}
    );
}

TEST_CASE("ADB transfer helpers reject incomplete requests", "[adb]") {
    const SdkInfo sdk;
    REQUIRE(PushAdbFile(sdk, {}, {}, {}).Error == AdbError::InvalidRequest);
    REQUIRE(PullAdbFile(sdk, {}, {}, {}).Error == AdbError::InvalidRequest);
    REQUIRE(UninstallAdbPackage(sdk, {}, {}, false).Error == AdbError::InvalidRequest);
}

TEST_CASE("ADB endpoint validation accepts supported forms", "[adb]") {
    REQUIRE(IsValidAdbNetworkEndpoint("192.168.1.10:5555"));
    REQUIRE(IsValidAdbNetworkEndpoint("[::1]:5555"));
    REQUIRE_FALSE(IsValidAdbNetworkEndpoint("localhost"));
    REQUIRE_FALSE(IsValidAdbNetworkEndpoint("localhost:70000"));

    REQUIRE(IsValidAdbSocketSpec("tcp:8080"));
    REQUIRE(IsValidAdbSocketSpec("localabstract:coredeck"));
    REQUIRE(IsValidAdbSocketSpec("jdwp:1234"));
    REQUIRE_FALSE(IsValidAdbSocketSpec("tcp:not-a-port"));
    REQUIRE_FALSE(IsValidAdbSocketSpec("unknown:value"));
}

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_APP_APP_CONFIG_TYPES_H
#define COREDECK_APP_APP_CONFIG_TYPES_H

#include <rfl/Rename.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace CoreDeck::App {
    constexpr int CONFIG_SCHEMA_VERSION = 2;
    constexpr int RECENT_APK_LIMIT = 4;

    enum class ThemePreference : uint8_t {
        System = 0,
        Dark = 1,
        Light = 2,
    };

    struct AdbDeviceProfile {
        rfl::Rename<"shell_commands", std::vector<std::string>> ShellCommands;
    };

    struct Environment {
        rfl::Rename<"sdk_path", std::string> SdkPath;
        rfl::Rename<"jdk_path", std::string> JdkPath;
    };

    struct Preferences {
        rfl::Rename<"onboarding_initialized", bool> OnboardingInitialized = false;
        rfl::Rename<"auto_scroll", bool> AutoScroll = true;
        rfl::Rename<"confirm_before_delete_avd", bool> ConfirmBeforeDeleteAvd = true;
        rfl::Rename<"crash_reporting_enabled", bool> CrashReportingEnabled = true;
        rfl::Rename<"check_for_updates_on_launch", bool> CheckForUpdatesOnLaunch = true;
        rfl::Rename<"include_beta_releases", bool> IncludeBetaReleases = false;
        rfl::Rename<"theme", int> Theme = static_cast<int>(ThemePreference::System);
        rfl::Rename<"language", std::string> Language;
    };

    struct Ui {
        struct Panels {
            rfl::Rename<"show_avd_list", bool> ShowAvdList = true;
            rfl::Rename<"show_options", bool> ShowOptions = true;
            rfl::Rename<"show_details", bool> ShowDetails = true;
            rfl::Rename<"show_log", bool> ShowLog = true;
        };

        struct Avd {
            rfl::Rename<"sort_mode", int> SortMode = 0;
            rfl::Rename<"sort_ascending", bool> SortAscending = true;
        };

        rfl::Rename<"panels", Panels> Panels;
        rfl::Rename<"avd", Avd> Avd;
    };

    struct History {
        rfl::Rename<"recent_apks", std::vector<std::string>> RecentApks;
    };

    struct Adb {
        rfl::Rename<"wireless_devices", std::vector<std::string>> WirelessDevices;
        rfl::Rename<"device_profiles", std::unordered_map<std::string, AdbDeviceProfile>> DeviceProfiles;
    };

    struct AppConfig {
        rfl::Rename<"schema_version", int> SchemaVersion = CONFIG_SCHEMA_VERSION;
        rfl::Rename<"environment", ::CoreDeck::App::Environment> Environment;
        rfl::Rename<"preferences", ::CoreDeck::App::Preferences> Preferences;
        rfl::Rename<"ui", ::CoreDeck::App::Ui> Ui;
        rfl::Rename<"history", ::CoreDeck::App::History> History;
        rfl::Rename<"adb", ::CoreDeck::App::Adb> Adb;
    };
}

#endif // COREDECK_APP_APP_CONFIG_TYPES_H

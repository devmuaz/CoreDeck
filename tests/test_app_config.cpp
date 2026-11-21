#include <catch2/catch_test_macros.hpp>

#include <rfl/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "app/app_config.h"
#include "app/app_paths.h"

using namespace CoreDeck::App;

namespace {
    class EnvVar {
    public:
        EnvVar(const char *name, const std::string &value) : m_Name(name) {
            if (const char *current = std::getenv(name)) {
                m_Previous = current;
            }
            Set(name, value);
        }

        ~EnvVar() {
            if (m_Previous) {
                Set(m_Name.c_str(), *m_Previous);
                return;
            }
#ifdef _WIN32
            Set(m_Name.c_str(), "");
#else
            unsetenv(m_Name.c_str());
#endif
        }

        EnvVar(const EnvVar &) = delete;
        EnvVar &operator=(const EnvVar &) = delete;

    private:
        static void Set(const char *name, const std::string &value) {
#ifdef _WIN32
            _putenv_s(name, value.c_str());
#else
            setenv(name, value.c_str(), 1);
#endif
        }

        std::string m_Name;
        std::optional<std::string> m_Previous;
    };

    class IsolatedConfigHome {
    public:
        explicit IsolatedConfigHome(const std::string &name) {
            m_Root = std::filesystem::temp_directory_path() / "coredeck-app-config-tests" / name;
            std::filesystem::remove_all(m_Root);
            std::filesystem::create_directories(m_Root);
#ifdef _WIN32
            m_UserProfile.emplace("USERPROFILE", m_Root.string());
            m_AppData.emplace("APPDATA", (m_Root / "AppData").string());
#else
            m_Home.emplace("HOME", m_Root.string());
#endif
        }

        ~IsolatedConfigHome() {
#ifdef _WIN32
            m_AppData.reset();
            m_UserProfile.reset();
#else
            m_Home.reset();
#endif
            std::error_code error;
            std::filesystem::remove_all(m_Root, error);
        }

        IsolatedConfigHome(const IsolatedConfigHome &) = delete;
        IsolatedConfigHome &operator=(const IsolatedConfigHome &) = delete;

        std::filesystem::path Directory() const {
#ifdef _WIN32
            return m_Root / "AppData" / "coredeck";
#else
            return m_Root / ".config" / "coredeck";
#endif
        }

    private:
        std::filesystem::path m_Root;
#ifdef _WIN32
        std::optional<EnvVar> m_UserProfile;
        std::optional<EnvVar> m_AppData;
#else
        std::optional<EnvVar> m_Home;
#endif
    };

    void RequireIsolated(const IsolatedConfigHome &home) {
        std::filesystem::create_directories(home.Directory());
        REQUIRE(std::filesystem::equivalent(home.Directory(), GetAppConfigPath("")));
    }

    void WriteFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << contents;
        REQUIRE(out.good());
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream in(path);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    constexpr const char *LEGACY_SETTINGS = R"({
        "SchemaVersion": 1,
        "AutoScroll": false,
        "ConfirmBeforeDeleteAvd": false,
        "CrashReportingEnabled": false,
        "CheckForUpdatesOnLaunch": false,
        "IncludeBetaReleases": true,
        "ShowAvdListPanel": false,
        "ShowOptionsPanel": false,
        "ShowDetailsPanel": false,
        "ShowLogPanel": false,
        "AvdSortMode": 2,
        "AvdSortAscending": false,
        "Theme": 2,
        "Language": "ar",
        "FirstRunComplete": true,
        "SdkPath": "/legacy/sdk",
        "JdkPath": "/legacy/jdk",
        "RecentApks": ["one.apk", "two.apk"],
        "AdbWirelessDevices": ["10.0.0.5:5555"],
        "AdbDeviceProfiles": [
            {"Serial": "emulator-5554", "ShellCommands": ["pwd"]},
            {"Serial": "", "ShellCommands": ["ignored"]},
            {"Serial": "emulator-5554", "ShellCommands": ["getprop", "ls"]}
        ]
    })";
}

TEST_CASE("App config round-trips nested snake_case keys", "[app_config]") {
    AppConfig config;
    config.SchemaVersion = CONFIG_SCHEMA_VERSION;
    config.Environment->SdkPath = "/sdk";
    config.Environment->JdkPath = "/jdk";
    config.Preferences->OnboardingInitialized = true;
    config.Preferences->AutoScroll = false;
    config.Preferences->CrashReportingEnabled = false;
    config.Preferences->Theme = static_cast<int>(ThemePreference::Dark);
    config.Preferences->Language = "ar";
    config.Ui->Panels->ShowAvdList = false;
    config.Ui->Avd->SortMode = 2;
    config.Ui->Avd->SortAscending = false;
    config.History->RecentApks = {"one.apk"};
    config.Adb->WirelessDevices = {"10.0.0.8:5555"};
    AdbDeviceProfile profile;
    profile.ShellCommands = {"getprop"};
    config.Adb->DeviceProfiles.value().emplace("emulator-5554", std::move(profile));

    const std::string json = rfl::json::write(config);
    REQUIRE(json.find("\"schema_version\":2") != std::string::npos);
    REQUIRE(json.find("\"environment\"") != std::string::npos);
    REQUIRE(json.find("\"sdk_path\":\"/sdk\"") != std::string::npos);
    REQUIRE(json.find("\"jdk_path\":\"/jdk\"") != std::string::npos);
    REQUIRE(json.find("\"onboarding_initialized\":true") != std::string::npos);
    REQUIRE(json.find("\"auto_scroll\":false") != std::string::npos);
    REQUIRE(json.find("\"crash_reporting_enabled\":false") != std::string::npos);
    REQUIRE(json.find("\"show_avd_list\":false") != std::string::npos);
    REQUIRE(json.find("\"sort_mode\":2") != std::string::npos);
    REQUIRE(json.find("\"sort_ascending\":false") != std::string::npos);
    REQUIRE(json.find("\"recent_apks\"") != std::string::npos);
    REQUIRE(json.find("\"wireless_devices\"") != std::string::npos);
    REQUIRE(json.find("\"device_profiles\"") != std::string::npos);
    REQUIRE(json.find("\"shell_commands\"") != std::string::npos);
    REQUIRE(json.find("\"Serial\"") == std::string::npos);
    REQUIRE(json.find("AdbDeviceProfiles") == std::string::npos);

    const AppConfig loaded = rfl::json::read<AppConfig, rfl::DefaultIfMissing>(json).value();
    REQUIRE(loaded.SchemaVersion.value() == CONFIG_SCHEMA_VERSION);
    REQUIRE(loaded.Environment->SdkPath.value() == "/sdk");
    REQUIRE(loaded.Environment->JdkPath.value() == "/jdk");
    REQUIRE(loaded.Preferences->OnboardingInitialized.value());
    REQUIRE_FALSE(loaded.Preferences->AutoScroll.value());
    REQUIRE_FALSE(loaded.Preferences->CrashReportingEnabled.value());
    REQUIRE(loaded.Preferences->Theme.value() == static_cast<int>(ThemePreference::Dark));
    REQUIRE(loaded.Preferences->Language.value() == "ar");
    REQUIRE_FALSE(loaded.Ui->Panels->ShowAvdList.value());
    REQUIRE(loaded.Ui->Avd->SortMode.value() == 2);
    REQUIRE_FALSE(loaded.Ui->Avd->SortAscending.value());
    REQUIRE(loaded.History->RecentApks.value() == std::vector<std::string>{"one.apk"});
    REQUIRE(loaded.Adb->WirelessDevices.value() == std::vector<std::string>{"10.0.0.8:5555"});
    const auto found = loaded.Adb->DeviceProfiles.value().find("emulator-5554");
    REQUIRE(found != loaded.Adb->DeviceProfiles.value().end());
    REQUIRE(found->second.ShellCommands.value() == std::vector<std::string>{"getprop"});
}

TEST_CASE("settings.json migrates into nested config.json", "[app_config]") {
    IsolatedConfigHome home("migrate");
    RequireIsolated(home);
    WriteFile(home.Directory() / "settings.json", LEGACY_SETTINGS);
    WriteFile(home.Directory() / "sdk_path", "/marker-sdk\n");
    WriteFile(home.Directory() / "apk-analyzer-recent.txt", "marker.apk\n");

    const AppConfig config = LoadAppConfig();
    REQUIRE(config.SchemaVersion.value() == CONFIG_SCHEMA_VERSION);
    REQUIRE(config.Environment->SdkPath.value() == "/legacy/sdk");
    REQUIRE(config.Environment->JdkPath.value() == "/legacy/jdk");
    REQUIRE(config.Preferences->OnboardingInitialized.value());
    REQUIRE_FALSE(config.Preferences->AutoScroll.value());
    REQUIRE_FALSE(config.Preferences->ConfirmBeforeDeleteAvd.value());
    REQUIRE_FALSE(config.Preferences->CrashReportingEnabled.value());
    REQUIRE_FALSE(config.Preferences->CheckForUpdatesOnLaunch.value());
    REQUIRE(config.Preferences->IncludeBetaReleases.value());
    REQUIRE(config.Preferences->Theme.value() == static_cast<int>(ThemePreference::Light));
    REQUIRE(config.Preferences->Language.value() == "ar");
    REQUIRE_FALSE(config.Ui->Panels->ShowAvdList.value());
    REQUIRE_FALSE(config.Ui->Panels->ShowOptions.value());
    REQUIRE_FALSE(config.Ui->Panels->ShowDetails.value());
    REQUIRE_FALSE(config.Ui->Panels->ShowLog.value());
    REQUIRE(config.Ui->Avd->SortMode.value() == 2);
    REQUIRE_FALSE(config.Ui->Avd->SortAscending.value());
    REQUIRE(config.History->RecentApks.value() == std::vector<std::string>{"one.apk", "two.apk"});
    REQUIRE(config.Adb->WirelessDevices.value() == std::vector<std::string>{"10.0.0.5:5555"});
    REQUIRE(config.Adb->DeviceProfiles.value().size() == 1);
    const auto found = config.Adb->DeviceProfiles.value().find("emulator-5554");
    REQUIRE(found != config.Adb->DeviceProfiles.value().end());
    REQUIRE(found->second.ShellCommands.value() == std::vector<std::string>{"getprop", "ls"});

    const std::filesystem::path configPath = home.Directory() / "config.json";
    REQUIRE(std::filesystem::is_regular_file(configPath));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "settings.json"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "config.json.tmp"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "sdk_path"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "apk-analyzer-recent.txt"));

    const std::string json = ReadFile(configPath);
    REQUIRE(json.find("\"schema_version\":2") != std::string::npos);
    REQUIRE(json.find("\"device_profiles\"") != std::string::npos);
    REQUIRE(json.find("\"shell_commands\"") != std::string::npos);
    REQUIRE(json.find("\"emulator-5554\"") != std::string::npos);
    REQUIRE(json.find("AdbDeviceProfiles") == std::string::npos);
    REQUIRE(json.find("\"Serial\"") == std::string::npos);

    const AppConfig reloaded = LoadAppConfig();
    REQUIRE(reloaded.Environment->SdkPath.value() == "/legacy/sdk");
    REQUIRE(reloaded.Adb->DeviceProfiles.value().size() == 1);
    REQUIRE(reloaded.Preferences->Language.value() == "ar");
}

TEST_CASE("existing config.json wins over settings.json", "[app_config]") {
    IsolatedConfigHome home("keep-config");
    RequireIsolated(home);
    WriteFile(
        home.Directory() / "config.json",
        R"({"schema_version":2,"environment":{"sdk_path":"/from-config","jdk_path":"/from-config-jdk"}})"
    );
    WriteFile(home.Directory() / "settings.json", R"({"SdkPath":"/from-settings","JdkPath":"/from-settings-jdk"})");

    const AppConfig config = LoadAppConfig();
    REQUIRE(config.Environment->SdkPath.value() == "/from-config");
    REQUIRE(config.Environment->JdkPath.value() == "/from-config-jdk");
    REQUIRE(std::filesystem::exists(home.Directory() / "settings.json"));
    REQUIRE(std::filesystem::exists(home.Directory() / "config.json"));
}

TEST_CASE("corrupt settings.json is left in place", "[app_config]") {
    IsolatedConfigHome home("corrupt-settings");
    RequireIsolated(home);
    WriteFile(home.Directory() / "settings.json", "{not json");

    const AppConfig config = LoadAppConfig();
    REQUIRE(config.Environment->SdkPath.value().empty());
    REQUIRE(config.SchemaVersion.value() == CONFIG_SCHEMA_VERSION);
    REQUIRE(std::filesystem::exists(home.Directory() / "settings.json"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "config.json"));
}

TEST_CASE("pre-json marker files become config.json", "[app_config]") {
    IsolatedConfigHome home("markers");
    RequireIsolated(home);
    WriteFile(home.Directory() / "sdk_path", "/marker-sdk\n");
    WriteFile(home.Directory() / "jdk_path", "/marker-jdk\n");
    WriteFile(home.Directory() / "first_run_complete", "1\n");
    WriteFile(home.Directory() / "apk-analyzer-recent.txt", "first.apk\nsecond.apk\n");

    const AppConfig config = LoadAppConfig();
    REQUIRE(config.Environment->SdkPath.value() == "/marker-sdk");
    REQUIRE(config.Environment->JdkPath.value() == "/marker-jdk");
    REQUIRE(config.Preferences->OnboardingInitialized.value());
    REQUIRE(config.History->RecentApks.value() == std::vector<std::string>{"first.apk", "second.apk"});
    REQUIRE(std::filesystem::is_regular_file(home.Directory() / "config.json"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "sdk_path"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "jdk_path"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "first_run_complete"));
    REQUIRE_FALSE(std::filesystem::exists(home.Directory() / "apk-analyzer-recent.txt"));
}

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <rfl/json.hpp>

#include "app/app_config.h"
#include "app/app_paths.h"
#include "platform/log.h"

namespace CoreDeck::App {
    namespace {
        std::string GetAppConfigFilePath() {
            return App::GetAppConfigPath("config.json");
        }

        std::string LegacySettingsPath() {
            return App::GetAppConfigPath("settings.json");
        }

        bool PathExists(const std::string &path) {
            if (path.empty()) {
                return false;
            }
            std::error_code error;
            return std::filesystem::exists(path, error) && !error;
        }

        std::string TrimTrailingSpace(std::string value) {
            while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            return value;
        }

        std::string ReadTrimmedLine(const std::string &path) {
            if (!PathExists(path)) {
                return {};
            }
            std::ifstream in(path);
            if (!in.is_open()) {
                return {};
            }
            std::string value;
            std::getline(in, value);
            return TrimTrailingSpace(std::move(value));
        }

        std::vector<std::string> ReadNonEmptyLines(const std::string &path) {
            std::vector<std::string> lines;
            if (!PathExists(path)) {
                return lines;
            }
            std::ifstream in(path);
            std::string line;
            while (lines.size() < RECENT_APK_LIMIT && std::getline(in, line)) {
                line = TrimTrailingSpace(std::move(line));
                if (!line.empty()) {
                    lines.push_back(std::move(line));
                }
            }
            return lines;
        }

        std::string ReadFileText(const std::string &path) {
            std::ifstream file(path);
            if (!file.is_open()) {
                return {};
            }
            return {
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>()
            };
        }

        void RemoveFileQuietly(const std::string &path) {
            if (path.empty()) {
                return;
            }
            std::error_code error;
            std::filesystem::remove(path, error);
        }

        bool ImportLegacyFiles(AppConfig &config) {
            bool changed = false;

            const std::string firstRun = App::GetAppConfigPath("first_run_complete");
            if (!config.Preferences->OnboardingInitialized.value() && PathExists(firstRun)) {
                config.Preferences->OnboardingInitialized = true;
                changed = true;
            }

            const std::string sdkPath = App::GetAppConfigPath("sdk_path");
            if (config.Environment->SdkPath.value().empty()) {
                if (std::string value = ReadTrimmedLine(sdkPath); !value.empty()) {
                    config.Environment->SdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string jdkPath = App::GetAppConfigPath("jdk_path");
            if (config.Environment->JdkPath.value().empty()) {
                if (std::string value = ReadTrimmedLine(jdkPath); !value.empty()) {
                    config.Environment->JdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string recentApks = App::GetAppConfigPath("apk-analyzer-recent.txt");
            if (config.History->RecentApks.value().empty()) {
                if (std::vector<std::string> lines = ReadNonEmptyLines(recentApks); !lines.empty()) {
                    config.History->RecentApks = std::move(lines);
                    changed = true;
                }
            }

            return changed;
        }

        void DeleteLegacyFiles() {
            RemoveFileQuietly(App::GetAppConfigPath("first_run_complete"));
            RemoveFileQuietly(App::GetAppConfigPath("sdk_path"));
            RemoveFileQuietly(App::GetAppConfigPath("jdk_path"));
            RemoveFileQuietly(App::GetAppConfigPath("apk-analyzer-recent.txt"));
        }

        // Field names match the flat v1 settings.json document.
        struct LegacyAdbDeviceProfile {
            std::string Serial;
            std::vector<std::string> ShellCommands;
        };

        struct LegacySettings {
            int SchemaVersion = 1;
            bool AutoScroll = true;
            bool ConfirmBeforeDeleteAvd = true;
            bool CrashReportingEnabled = true;
            bool CheckForUpdatesOnLaunch = true;
            bool IncludeBetaReleases = false;
            bool ShowAvdListPanel = true;
            bool ShowOptionsPanel = true;
            bool ShowDetailsPanel = true;
            bool ShowLogPanel = true;
            int AvdSortMode = 0;
            bool AvdSortAscending = true;
            int Theme = static_cast<int>(ThemePreference::System);
            std::string Language;
            bool FirstRunComplete = false;
            std::string SdkPath;
            std::string JdkPath;
            std::vector<std::string> RecentApks;
            std::vector<std::string> AdbWirelessDevices;
            std::vector<LegacyAdbDeviceProfile> AdbDeviceProfiles;
            int FridaScriptRuntime = 0;
            int FridaScriptMode = 0;
            std::string FridaExistingPort = "27042";
            std::string FridaServerCachePath;
            std::string FridaProjectPath;
            std::string FridaScriptsPath;
        };

        AppConfig MapLegacySettings(const LegacySettings &legacy) {
            AppConfig config;
            config.SchemaVersion = CONFIG_SCHEMA_VERSION;
            config.Environment->SdkPath = legacy.SdkPath;
            config.Environment->JdkPath = legacy.JdkPath;
            config.Preferences->OnboardingInitialized = legacy.FirstRunComplete;
            config.Preferences->AutoScroll = legacy.AutoScroll;
            config.Preferences->ConfirmBeforeDeleteAvd = legacy.ConfirmBeforeDeleteAvd;
            config.Preferences->CrashReportingEnabled = legacy.CrashReportingEnabled;
            config.Preferences->CheckForUpdatesOnLaunch = legacy.CheckForUpdatesOnLaunch;
            config.Preferences->IncludeBetaReleases = legacy.IncludeBetaReleases;
            config.Preferences->Theme = legacy.Theme;
            config.Preferences->Language = legacy.Language;
            config.Ui->Panels->ShowAvdList = legacy.ShowAvdListPanel;
            config.Ui->Panels->ShowOptions = legacy.ShowOptionsPanel;
            config.Ui->Panels->ShowDetails = legacy.ShowDetailsPanel;
            config.Ui->Panels->ShowLog = legacy.ShowLogPanel;
            config.Ui->Avd->SortMode = legacy.AvdSortMode;
            config.Ui->Avd->SortAscending = legacy.AvdSortAscending;
            config.History->RecentApks = legacy.RecentApks;
            config.Adb->WirelessDevices = legacy.AdbWirelessDevices;
            for (const LegacyAdbDeviceProfile &profile: legacy.AdbDeviceProfiles) {
                if (profile.Serial.empty()) {
                    continue;
                }
                AdbDeviceProfile mapped;
                mapped.ShellCommands = profile.ShellCommands;
                config.Adb->DeviceProfiles.value().insert_or_assign(profile.Serial, std::move(mapped));
            }
            config.Frida->Runtime = legacy.FridaScriptRuntime;
            config.Frida->Mode = legacy.FridaScriptMode;
            config.Frida->ExistingPort = legacy.FridaExistingPort;
            config.Frida->ServerCachePath = legacy.FridaServerCachePath;
            config.Frida->ProjectPath = legacy.FridaProjectPath;
            config.Frida->ScriptsPath = legacy.FridaScriptsPath;
            return config;
        }

        std::optional<LegacySettings> ReadLegacySettings(const std::string &path) {
            const std::string json = ReadFileText(path);
            if (json.empty()) {
                Platform::Log::Error("Legacy settings file is empty: ", path);
                return std::nullopt;
            }

            auto parsed = rfl::json::read<LegacySettings, rfl::DefaultIfMissing>(json);
            if (!parsed.has_value()) {
                Platform::Log::Error("Failed to parse legacy settings file ", path, ": ", parsed.error().what());
                return std::nullopt;
            }
            return std::move(parsed).value();
        }

        AppConfig ReadConfigFile() {
            const std::string path = GetAppConfigFilePath();
            if (!PathExists(path)) {
                return {};
            }

            const std::string json = ReadFileText(path);
            if (json.empty()) {
                return {};
            }
            return rfl::json::read<AppConfig, rfl::DefaultIfMissing>(json).value();
        }

        // Reads settings.json when config.json is absent, writes schema 2, then removes the old file.
        std::optional<AppConfig> MigrateLegacyConfigFile() {
            const std::string legacyPath = LegacySettingsPath();
            const std::string configPath = GetAppConfigFilePath();
            if (!PathExists(legacyPath) || PathExists(configPath)) {
                return std::nullopt;
            }

            try {
                const std::optional<LegacySettings> legacy = ReadLegacySettings(legacyPath);
                if (!legacy) {
                    return std::nullopt;
                }

                AppConfig config = MapLegacySettings(*legacy);
                if (!SaveAppConfig(config)) {
                    return std::nullopt;
                }
                RemoveFileQuietly(legacyPath);
                Platform::Log::Info("Migrated settings.json to config.json");
                return config;
            } catch (const std::exception &e) {
                Platform::Log::Error("Failed to migrate settings.json: ", e.what());
                return std::nullopt;
            }
        }
    }

    bool SaveAppConfig(const AppConfig &config) {
        try {
            const std::string path = GetAppConfigFilePath();
            if (path.empty()) {
                return false;
            }

            const std::filesystem::path destination(path);
            std::error_code error;
            std::filesystem::create_directories(destination.parent_path(), error);
            if (error) {
                Platform::Log::Error("Failed to create app config directory: ", error.message());
                return false;
            }

            AppConfig stored = config;
            stored.SchemaVersion = CONFIG_SCHEMA_VERSION;
            const std::string json = rfl::json::write(stored);
            const std::filesystem::path temporary = destination.parent_path() / (destination.filename().string() + ".tmp");
            {
                std::ofstream file(temporary);
                if (!file.is_open()) {
                    Platform::Log::Error("Failed to save app config to: ", path);
                    return false;
                }
                file << json;
                file.close();
                if (!file.good()) {
                    RemoveFileQuietly(temporary.string());
                    Platform::Log::Error("Failed to save app config to: ", path);
                    return false;
                }
            }

            std::filesystem::rename(temporary, destination, error);
#ifdef _WIN32
            // std::filesystem::rename does not replace an existing file on Windows.
            if (error) {
                std::error_code removeError;
                std::filesystem::remove(destination, removeError);
                if (!removeError) {
                    error.clear();
                    std::filesystem::rename(temporary, destination, error);
                }
            }
#endif
            if (error) {
                RemoveFileQuietly(temporary.string());
                Platform::Log::Error("Failed to replace app config: ", error.message());
                return false;
            }
            return true;
        } catch (const std::exception &e) {
            Platform::Log::Error("Failed to save app config: ", e.what());
            return false;
        }
    }

    AppConfig LoadAppConfig() {
        AppConfig config;
        try {
            if (std::optional<AppConfig> migrated = MigrateLegacyConfigFile()) {
                config = std::move(*migrated);
            } else if (PathExists(GetAppConfigFilePath())) {
                config = ReadConfigFile();
            }
        } catch (const std::exception &e) {
            Platform::Log::Error("Failed to load app config: ", e.what());
            config = {};
        }

        if (!ImportLegacyFiles(config)) {
            DeleteLegacyFiles();
            return config;
        }
        if (!SaveAppConfig(config)) {
            return config;
        }
        DeleteLegacyFiles();
        return config;
    }
}

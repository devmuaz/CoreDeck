//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <filesystem>
#include <fstream>
#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "app_settings.h"
#include "paths.h"
#include "log.h"

namespace CoreDeck {
    namespace {
        constexpr int RECENT_APK_LIMIT = 4;

        std::string GetAppSettingsFilePath() {
            return Paths::GetAppConfigPath("settings.json");
        }

        std::string TrimTrailingSpace(std::string value) {
            while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            return value;
        }

        std::string ReadTrimmedLine(const std::string &path) {
            if (path.empty() || !std::filesystem::exists(path)) {
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
            if (path.empty() || !std::filesystem::exists(path)) {
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

        void RemoveFileQuietly(const std::string &path) {
            if (path.empty()) {
                return;
            }
            std::error_code error;
            std::filesystem::remove(path, error);
        }

        bool ImportLegacyFiles(AppSettings &settings) {
            bool changed = false;

            const std::string firstRun = Paths::GetAppConfigPath("first_run_complete");
            if (!settings.FirstRunComplete && !firstRun.empty() && std::filesystem::exists(firstRun)) {
                settings.FirstRunComplete = true;
                changed = true;
            }

            const std::string sdkPath = Paths::GetAppConfigPath("sdk_path");
            if (settings.SdkPath.empty()) {
                if (std::string value = ReadTrimmedLine(sdkPath); !value.empty()) {
                    settings.SdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string jdkPath = Paths::GetAppConfigPath("jdk_path");
            if (settings.JdkPath.empty()) {
                if (std::string value = ReadTrimmedLine(jdkPath); !value.empty()) {
                    settings.JdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string recentApks = Paths::GetAppConfigPath("apk-analyzer-recent.txt");
            if (settings.RecentApks.empty()) {
                if (std::vector<std::string> lines = ReadNonEmptyLines(recentApks); !lines.empty()) {
                    settings.RecentApks = std::move(lines);
                    changed = true;
                }
            }

            return changed;
        }

        void DeleteLegacyFiles() {
            RemoveFileQuietly(Paths::GetAppConfigPath("first_run_complete"));
            RemoveFileQuietly(Paths::GetAppConfigPath("sdk_path"));
            RemoveFileQuietly(Paths::GetAppConfigPath("jdk_path"));
            RemoveFileQuietly(Paths::GetAppConfigPath("apk-analyzer-recent.txt"));
        }

        AppSettings ReadSettingsFile() {
            const std::string path = GetAppSettingsFilePath();
            if (path.empty() || !std::filesystem::exists(path)) {
                return {};
            }

            std::ifstream file(path);
            if (!file.is_open()) {
                return {};
            }

            const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (json.empty()) {
                return {};
            }

            return rfl::json::read<AppSettings, rfl::DefaultIfMissing>(json).value();
        }
    }

    AppSettings LoadAppSettings() {
        AppSettings settings;
        try {
            settings = ReadSettingsFile();
        } catch (const std::exception &e) {
            Log::Error("Failed to load app settings: ", e.what());
            settings = {};
        }

        if (!ImportLegacyFiles(settings)) {
            DeleteLegacyFiles();
            return settings;
        }
        if (!SaveAppSettings(settings)) {
            return settings;
        }
        DeleteLegacyFiles();
        return settings;
    }

    bool SaveAppSettings(const AppSettings &settings) {
        try {
            const std::string path = GetAppSettingsFilePath();
            if (path.empty()) {
                return false;
            }

            std::filesystem::path fsPath(path);
            std::error_code ec;
            std::filesystem::create_directories(fsPath.parent_path(), ec);
            if (ec) {
                Log::Error("Failed to create app config directory: ", ec.message());
                return false;
            }

            const auto json = rfl::json::write(settings);
            std::ofstream file(path);
            if (!file.is_open()) {
                Log::Error("Failed to save app settings to: ", path);
                return false;
            }
            file << json;
            return file.good();
        } catch (const std::exception &e) {
            Log::Error("Failed to save app settings: ", e.what());
            return false;
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <filesystem>
#include <fstream>
#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "app/app_settings.h"
#include "app/paths.h"
#include "platform/log.h"

namespace CoreDeck::App {
    namespace {
        std::string GetAppSettingsFilePath() {
            return App::GetAppConfigPath("settings.json");
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

            const std::string firstRun = App::GetAppConfigPath("first_run_complete");
            if (!settings.FirstRunComplete && !firstRun.empty() && std::filesystem::exists(firstRun)) {
                settings.FirstRunComplete = true;
                changed = true;
            }

            const std::string sdkPath = App::GetAppConfigPath("sdk_path");
            if (settings.SdkPath.empty()) {
                if (std::string value = ReadTrimmedLine(sdkPath); !value.empty()) {
                    settings.SdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string jdkPath = App::GetAppConfigPath("jdk_path");
            if (settings.JdkPath.empty()) {
                if (std::string value = ReadTrimmedLine(jdkPath); !value.empty()) {
                    settings.JdkPath = std::move(value);
                    changed = true;
                }
            }

            const std::string recentApks = App::GetAppConfigPath("apk-analyzer-recent.txt");
            if (settings.RecentApks.empty()) {
                if (std::vector<std::string> lines = ReadNonEmptyLines(recentApks); !lines.empty()) {
                    settings.RecentApks = std::move(lines);
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
            Platform::Log::Error("Failed to load app settings: ", e.what());
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

            const std::filesystem::path fsPath(path);
            std::error_code ec;
            std::filesystem::create_directories(fsPath.parent_path(), ec);
            if (ec) {
                Platform::Log::Error("Failed to create app config directory: ", ec.message());
                return false;
            }

            const auto json = rfl::json::write(settings);
            std::ofstream file(path);
            if (!file.is_open()) {
                Platform::Log::Error("Failed to save app settings to: ", path);
                return false;
            }
            file << json;
            return file.good();
        } catch (const std::exception &e) {
            Platform::Log::Error("Failed to save app settings: ", e.what());
            return false;
        }
    }
}

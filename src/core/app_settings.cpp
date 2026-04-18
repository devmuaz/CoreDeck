//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <filesystem>
#include <fstream>
#include <iostream>
#include <rfl/json.hpp>

#include "app_settings.h"
#include "paths.h"

namespace CoreDeck {
    static std::string GetAppSettingsFilePath() {
        return Paths::GetAppConfigPath("settings.json");
    }

    AppSettings LoadAppSettings() {
        try {
            const std::string path = GetAppSettingsFilePath();
            if (path.empty() || !std::filesystem::exists(path)) return AppSettings{};

            std::ifstream file(path);
            if (!file.is_open()) return AppSettings{};

            const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            if (json.empty()) return AppSettings{};

            return rfl::json::read<AppSettings>(json).value();
        } catch (const std::exception &e) {
            std::cerr << "Failed to load app settings: " << e.what() << std::endl;
            return AppSettings{};
        }
    }

    void SaveAppSettings(const AppSettings &settings) {
        try {
            const std::string path = GetAppSettingsFilePath();
            if (path.empty()) return;

            std::filesystem::path fsPath(path);
            std::error_code ec;
            std::filesystem::create_directories(fsPath.parent_path(), ec);
            if (ec) {
                std::cerr << "Failed to create app config directory: " << ec.message() << std::endl;
                return;
            }

            const auto json = rfl::json::write(settings);
            std::ofstream file(path);
            if (!file.is_open()) {
                std::cerr << "Failed to save app settings to: " << path << std::endl;
                return;
            }
            file << json;
        } catch (const std::exception &e) {
            std::cerr << "Failed to save app settings: " << e.what() << std::endl;
        }
    }
}

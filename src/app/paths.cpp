//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#include "app/paths.h"

#include <filesystem>

#include "platform/log.h"
#include "platform/paths.h"
#include "app/constants.h"

namespace CoreDeck::App {
    std::string GetAppConfigPath(const std::string &subPath) {
        const std::string configDir = Platform::GetConfigDirectory();
        if (configDir.empty()) {
            return "";
        }

        if (subPath.empty()) {
            return Platform::JoinPaths({configDir, APP_CONFIG_DIR});
        }
        return Platform::JoinPaths({configDir, APP_CONFIG_DIR, subPath});
    }
    std::string GetResourcesDirectory() {
        std::string exeDir = Platform::GetExecutableDirectory();
        if (exeDir.empty()) {
            return {};
        }
#ifdef __APPLE__
        const std::filesystem::path p(exeDir);
        if (p.filename() == "MacOS" && p.parent_path().filename() == "Contents") {
            return (p.parent_path() / "Resources").string();
        }
#endif
        return std::move(exeDir);
    }

    std::string GetAssetsDirectory() {
        const char *overrideDir = std::getenv(ENV_COREDECK_ASSETS_DIR); // NOLINT(concurrency-mt-unsafe)
        if (overrideDir != nullptr && overrideDir[0] != '\0') {
            std::error_code error;
            if (!std::filesystem::is_directory(overrideDir, error)) {
                Platform::Log::Warn(ENV_COREDECK_ASSETS_DIR, " is not a directory: ", overrideDir);
            }
            return {overrideDir};
        }
        return Platform::JoinPaths({GetResourcesDirectory(), "assets"});
    }
    std::string GetOptionsConfigPath(const std::string &avdName) {
        const std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) {
            return "";
        }

        return Platform::JoinPaths({configPath, avdName + ".json"});
    }

    std::string EnsureOptionsConfigDirectoryExists() {
        std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) {
            return "";
        }

        try {
            std::filesystem::create_directories(configPath);
            return configPath;
        } catch (const std::exception &e) {
            Platform::Log::Error("Failed to create config directory: ", e.what());
            return "";
        }
    }
}

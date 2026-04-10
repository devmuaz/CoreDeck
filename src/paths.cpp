//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#include "paths.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace CoreDeck::Paths {
    Platform GetCurrentPlatform() {
#ifdef _WIN32
        return Platform::Windows;
#elif defined(__APPLE__)
        return Platform::macOS;
#elif defined(__linux__)
        return Platform::Linux;
#else
        return Platform::Unknown;
#endif
    }

    const char *GetPlatformName() {
        switch (GetCurrentPlatform()) {
            case Platform::Windows: return "Windows";
            case Platform::macOS: return "macOS";
            case Platform::Linux: return "Linux";
            default: return "Unknown";
        }
    }

    std::string GetHomeDirectory() {
        // ReSharper disable once CppTooWideScope
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows: {
                const char *userProfile = std::getenv("USERPROFILE");
                if (userProfile) return std::string(userProfile);

                const char *homeDrive = std::getenv("HOMEDRIVE");
                const char *homePath = std::getenv("HOMEPATH");
                if (homeDrive && homePath) {
                    return std::string(homeDrive) + std::string(homePath);
                }
                break;
            }
            case Platform::macOS:
            case Platform::Linux: {
                const char *home = std::getenv("HOME");
                if (home) return {home};
                break;
            }
            default:
                break;
        }

        std::cerr << "Warning: Could not determine home directory" << std::endl;
        return "";
    }

    std::string GetConfigDirectory() {
        const auto platform = GetCurrentPlatform();
        const std::string home = GetHomeDirectory();

        if (home.empty()) return "";

        switch (platform) {
            case Platform::Windows: {
                const char *appData = std::getenv("APPDATA");
                if (appData) return std::string(appData);
                return JoinPaths({home, "AppData", "Roaming"});
            }
            case Platform::macOS:
            case Platform::Linux:
                return JoinPaths({home, ".config"});
            default:
                return JoinPaths({home, ".config"}); // fallback
        }
    }

    std::string GetAndroidSdkDefaultPath() {
        const auto platform = GetCurrentPlatform();
        const std::string home = GetHomeDirectory();

        if (home.empty()) return "";

        switch (platform) {
            case Platform::Windows: {
                const char *localAppData = std::getenv("LOCALAPPDATA");
                if (localAppData) {
                    return JoinPaths({std::string(localAppData), "Android", "Sdk"});
                }
                return JoinPaths({home, "AppData", "Local", "Android", "Sdk"});
            }
            case Platform::macOS:
                return JoinPaths({home, "Library", "Android", "sdk"});
            case Platform::Linux:
                return JoinPaths({home, "Android", "Sdk"});
            default:
                return JoinPaths({home, "Android", "Sdk"}); // fallback
        }
    }

    std::string GetAvdDirectory() {
        const std::string home = GetHomeDirectory();
        if (home.empty()) return "";

        return JoinPaths({home, ".android", "avd"});
    }

    std::string GetAppConfigPath(const std::string &subPath) {
        const std::string configDir = GetConfigDirectory();
        if (configDir.empty()) return "";

        if (subPath.empty()) {
            return JoinPaths({configDir, "coredeck"});
        }
        return JoinPaths({configDir, "coredeck", subPath});
    }

    std::string GetNullDevice() {
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows:
                return "NUL";
            case Platform::macOS:
            case Platform::Linux:
            default:
                return "/dev/null";
        }
    }

    std::string GetExecutableExtension() {
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows:
                return ".exe";
            case Platform::macOS:
            case Platform::Linux:
            default:
                return "";
        }
    }

    std::string JoinPaths(const std::vector<std::string> &components) {
        if (components.empty()) return "";

        std::filesystem::path result;
        for (const auto &component: components) {
            if (!component.empty()) {
                result /= component;
            }
        }
        return result.string();
    }

    std::string NormalizePath(const std::string &path) {
        try {
            return std::filesystem::path(path).lexically_normal().string();
        } catch (const std::exception &) {
            return path;
        }
    }

    std::string GetOptionsConfigPath(const std::string &avdName) {
        const std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) return "";

        return JoinPaths({configPath, avdName + ".json"});
    }

    std::string EnsureConfigDirectoryExists() {
        const std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) return "";

        try {
            std::filesystem::create_directories(configPath);
            return configPath;
        } catch (const std::exception &e) {
            std::cerr << "Failed to create config directory: " << e.what() << std::endl;
            return "";
        }
    }
}

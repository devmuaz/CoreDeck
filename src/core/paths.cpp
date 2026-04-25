//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#include "paths.h"
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

#include "log.h"

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

        Log::Error("Could not determine home directory");
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

    std::string GetExecutableDirectory() {
#ifdef _WIN32
        char buffer[MAX_PATH];
        const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (len == 0) return {};
        return std::filesystem::path(std::string(buffer, len)).parent_path().string();
#elif defined(__APPLE__)
        char buffer[PATH_MAX];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) != 0) return {};
        return std::filesystem::canonical(buffer).parent_path().string();
#elif defined(__linux__)
        char buffer[PATH_MAX];
        const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len <= 0) return {};
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path().string();
#else
        return {};
#endif
    }

    std::string GetResourcesDirectory() {
        std::string exeDir = GetExecutableDirectory();
        if (exeDir.empty()) return {};
#ifdef __APPLE__
        const std::filesystem::path p(exeDir);
        if (p.filename() == "MacOS" && p.parent_path().filename() == "Contents") {
            return (p.parent_path() / "Resources").string();
        }
#endif
        return std::move(exeDir);
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

    std::string EnsureOptionsConfigDirectoryExists() {
        std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) return "";

        try {
            std::filesystem::create_directories(configPath);
            return configPath;
        } catch (const std::exception &e) {
            Log::Error("Failed to create config directory: ", e.what());
            return "";
        }
    }

    namespace Onboarding {
        static constexpr const char *FIRST_RUN_FLAG_FILE = "first_run_complete";
        static constexpr const char *SDK_OVERRIDE_PATH = "sdk_path";

        bool IsFirstRunComplete() {
            const std::string path = Paths::GetAppConfigPath(FIRST_RUN_FLAG_FILE);
            return std::filesystem::exists(path);
        }

        void MarkFirstRunComplete() {
            const std::string path = Paths::GetAppConfigPath(FIRST_RUN_FLAG_FILE);
            std::ofstream out(path);
        }

        bool ValidateSdkPath(const std::string &path) {
            if (path.empty()) return false;
            if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) return false;

            const std::string emulatorBinary = Paths::JoinPaths({
                path, "emulator", "emulator" + Paths::GetExecutableExtension()
            });
            return std::filesystem::exists(emulatorBinary);
        }

        std::string LoadSdkPathOverride() {
            const std::string path = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
            if (!std::filesystem::exists(path)) return {};

            std::ifstream in(path);
            std::string value;
            std::getline(in, value);
            return value;
        }

        void SaveSdkPathOverride(const std::string &path) {
            const std::string file = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
            std::ofstream out(file);
            out << path;
        }

        void ClearSdkPathOverride() {
            const std::string file = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }
}

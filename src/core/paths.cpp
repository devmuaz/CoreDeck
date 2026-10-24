//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#include "paths.h"
#include <filesystem>

#include "app_settings.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#elif __linux__
#include <unistd.h>
#endif

#include "constants.h"
#include "log.h"

namespace CoreDeck::Paths {
    Platform GetCurrentPlatform() {
#ifdef _WIN32
        return Platform::Windows;
#elif __APPLE__
        return Platform::MacOS;
#elif __linux__
        return Platform::Linux;
#else
        return Platform::Unknown;
#endif
    }

    const char *GetPlatformName() {
        switch (GetCurrentPlatform()) {
            case Platform::Windows:
                return "Windows";
            case Platform::MacOS:
                return "macOS";
            case Platform::Linux:
                return "Linux";
            default:
                return "Unknown";
        }
    }

    std::string GetHomeDirectory() {
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows: {
                const char *userProfile = std::getenv(Core::ENV_USERPROFILE); // NOLINT(concurrency-mt-unsafe)
                if (userProfile) {
                    return {userProfile};
                }

                const char *homeDrive = std::getenv(Core::ENV_HOMEDRIVE); // NOLINT(concurrency-mt-unsafe)
                const char *homePath = std::getenv(Core::ENV_HOMEPATH); // NOLINT(concurrency-mt-unsafe)
                if (homeDrive && homePath) {
                    return std::string(homeDrive) + std::string(homePath);
                }
                break;
            }
            case Platform::MacOS:
            case Platform::Linux: {
                const char *home = std::getenv(Core::ENV_HOME); // NOLINT(concurrency-mt-unsafe)
                if (home) {
                    return {home};
                }
                break;
            }
            default:
                break;
        }

        Core::Log::Error("Could not determine home directory");
        return "";
    }

    std::string GetConfigDirectory() {
        const auto platform = GetCurrentPlatform();
        const std::string home = GetHomeDirectory();

        if (home.empty()) {
            return "";
        }

        switch (platform) {
            case Platform::Windows: {
                const char *appData = std::getenv(Core::ENV_APPDATA); // NOLINT(concurrency-mt-unsafe)
                if (appData) {
                    return {appData};
                }
                return JoinPaths({home, "AppData", "Roaming"});
            }
            case Platform::MacOS:
            case Platform::Linux:
            default:
                return JoinPaths({home, ".config"});
        }
    }

    std::string GetAndroidSdkDefaultPath() {
        const auto platform = GetCurrentPlatform();
        const std::string home = GetHomeDirectory();

        if (home.empty()) {
            return "";
        }

        switch (platform) {
            case Platform::Windows: {
                const char *localAppData = std::getenv(Core::ENV_LOCALAPPDATA); // NOLINT(concurrency-mt-unsafe)
                if (localAppData) {
                    return JoinPaths({std::string(localAppData), "Android", "Sdk"});
                }
                return JoinPaths({home, "AppData", "Local", "Android", "Sdk"});
            }
            case Platform::MacOS:
                return JoinPaths({home, "Library", "Android", "sdk"});
            case Platform::Linux:
            default:
                return JoinPaths({home, "Android", "Sdk"});
        }
    }

    std::string GetAvdDirectory() {
        const std::string home = GetHomeDirectory();
        if (home.empty()) {
            return "";
        }

        return JoinPaths({home, ".android", "avd"});
    }

    std::string GetAppConfigPath(const std::string &subPath) {
        const std::string configDir = GetConfigDirectory();
        if (configDir.empty()) {
            return "";
        }

        if (subPath.empty()) {
            return JoinPaths({configDir, "coredeck"});
        }
        return JoinPaths({configDir, "coredeck", subPath});
    }

    std::string_view GetNullDevice() {
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows:
                return "NUL";
            case Platform::MacOS:
            case Platform::Linux:
            default:
                return "/dev/null";
        }
    }

    std::string_view GetExecutableExtension() {
        const auto platform = GetCurrentPlatform();

        switch (platform) {
            case Platform::Windows:
                return ".exe";
            case Platform::MacOS:
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
#elif __APPLE__
        char buffer[PATH_MAX];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) != 0) {
            return {};
        }
        return std::filesystem::canonical(buffer).parent_path().string();
#elif __linux__
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
        const char *overrideDir = std::getenv(Core::ENV_COREDECK_ASSETS_DIR); // NOLINT(concurrency-mt-unsafe)
        if (overrideDir != nullptr && overrideDir[0] != '\0') {
            std::error_code error;
            if (!std::filesystem::is_directory(overrideDir, error)) {
                Core::Log::Warn(Core::ENV_COREDECK_ASSETS_DIR, " is not a directory: ", overrideDir);
            }
            return {overrideDir};
        }
        return JoinPaths({GetResourcesDirectory(), "assets"});
    }

    std::string JoinPaths(const std::vector<std::string> &components) {
        if (components.empty()) {
            return "";
        }

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

    bool SameFilePath(const std::string &left, const std::string &right) {
        if (left.empty() || right.empty()) {
            return false;
        }

        const std::filesystem::path input(NormalizePath(left));
        const std::filesystem::path output(NormalizePath(right));
        if (input == output) {
            return true;
        }

        std::error_code inputError;
        std::error_code outputError;
        const std::filesystem::path inputCanonical = std::filesystem::weakly_canonical(input, inputError);
        const std::filesystem::path outputCanonical = std::filesystem::weakly_canonical(output, outputError);
        if (!inputError && !outputError && inputCanonical == outputCanonical) {
            return true;
        }

        if (!std::filesystem::exists(input, inputError) || !std::filesystem::exists(output, outputError)) {
            return false;
        }
        std::error_code equivalentError;
        return std::filesystem::equivalent(input, output, equivalentError);
    }

    std::string GetOptionsConfigPath(const std::string &avdName) {
        const std::string configPath = GetAppConfigPath("avd-options");
        if (configPath.empty()) {
            return "";
        }

        return JoinPaths({configPath, avdName + ".json"});
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
            Core::Log::Error("Failed to create config directory: ", e.what());
            return "";
        }
    }

    namespace Onboarding {
        bool IsFirstRunComplete() {
            return Core::LoadAppSettings().FirstRunComplete;
        }

        void MarkFirstRunComplete() {
            Core::UpdateAppSettings([](Core::AppSettings &settings) {
                settings.FirstRunComplete = true;
            });
        }

        bool ValidateSdkPath(const std::string &path) {
            if (path.empty()) {
                return false;
            }
            if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
                return false;
            }

            const std::string emulatorBinary = Paths::JoinPaths({path, "emulator", Paths::WithExecutableExtension("emulator")});
            return std::filesystem::exists(emulatorBinary);
        }

        std::string LoadSdkPathOverride() {
            return Core::LoadAppSettings().SdkPath;
        }

        void SaveSdkPathOverride(const std::string &path) {
            Core::UpdateAppSettings([&path](Core::AppSettings &settings) {
                settings.SdkPath = path;
            });
        }

        void ClearSdkPathOverride() {
            Core::UpdateAppSettings([](Core::AppSettings &settings) {
                settings.SdkPath.clear();
            });
        }

        std::string LoadJdkPathOverride() {
            return Core::LoadAppSettings().JdkPath;
        }

        void SaveJdkPathOverride(const std::string &path) {
            Core::UpdateAppSettings([&path](Core::AppSettings &settings) {
                settings.JdkPath = path;
            });
        }

        void ClearJdkPathOverride() {
            Core::UpdateAppSettings([](Core::AppSettings &settings) {
                settings.JdkPath.clear();
            });
        }
    }
}

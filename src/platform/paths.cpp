//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#include "paths.h"

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#elif __linux__
#include <unistd.h>
#endif

#include "env.h"
#include "log.h"

namespace CoreDeck::Platform {
    Os GetCurrentOs() {
#ifdef _WIN32
        return Os::Windows;
#elif __APPLE__
        return Os::MacOS;
#elif __linux__
        return Os::Linux;
#else
        return Os::Unknown;
#endif
    }

    const char *GetOsName() {
        switch (GetCurrentOs()) {
            case Os::Windows:
                return "Windows";
            case Os::MacOS:
                return "macOS";
            case Os::Linux:
                return "Linux";
            default:
                return "Unknown";
        }
    }

    std::string GetHomeDirectory() {
        const auto platform = GetCurrentOs();

        switch (platform) {
            case Os::Windows: {
                const char *userProfile = std::getenv(ENV_USERPROFILE); // NOLINT(concurrency-mt-unsafe)
                if (userProfile) {
                    return {userProfile};
                }

                const char *homeDrive = std::getenv(ENV_HOMEDRIVE); // NOLINT(concurrency-mt-unsafe)
                const char *homePath = std::getenv(ENV_HOMEPATH); // NOLINT(concurrency-mt-unsafe)
                if (homeDrive && homePath) {
                    return std::string(homeDrive) + std::string(homePath);
                }
                break;
            }
            case Os::MacOS:
            case Os::Linux: {
                const char *home = std::getenv(ENV_HOME); // NOLINT(concurrency-mt-unsafe)
                if (home) {
                    return {home};
                }
                break;
            }
            default:
                break;
        }

        Log::Error("Could not determine home directory");
        return "";
    }

    std::string GetConfigDirectory() {
        const auto platform = GetCurrentOs();
        const std::string home = GetHomeDirectory();

        if (home.empty()) {
            return "";
        }

        switch (platform) {
            case Os::Windows: {
                const char *appData = std::getenv(ENV_APPDATA); // NOLINT(concurrency-mt-unsafe)
                if (appData) {
                    return {appData};
                }
                return JoinPaths({home, "AppData", "Roaming"});
            }
            case Os::MacOS:
            case Os::Linux:
            default:
                return JoinPaths({home, ".config"});
        }
    }
    std::string_view GetNullDevice() {
        const auto platform = GetCurrentOs();

        switch (platform) {
            case Os::Windows:
                return "NUL";
            case Os::MacOS:
            case Os::Linux:
            default:
                return "/dev/null";
        }
    }

    std::string_view GetExecutableExtension() {
        const auto platform = GetCurrentOs();

        switch (platform) {
            case Os::Windows:
                return ".exe";
            case Os::MacOS:
            case Os::Linux:
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
}

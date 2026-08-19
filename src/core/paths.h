//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#ifndef COREDECK_PATHS_H
#define COREDECK_PATHS_H

#include <string>
#include <vector>

namespace CoreDeck::Paths {
    enum class Platform : uint8_t {
        Windows,
        MacOS,
        Linux,
        Unknown
    };

    Platform GetCurrentPlatform();

    const char *GetPlatformName();

    std::string GetHomeDirectory();

    std::string GetConfigDirectory();

    std::string GetAndroidSdkDefaultPath();

    std::string GetAvdDirectory();

    std::string GetAppConfigPath(const std::string &subPath = "");

    std::string GetNullDevice();

    std::string GetExecutableExtension();

    std::string GetExecutableDirectory();

    std::string GetResourcesDirectory();

    std::string JoinPaths(const std::vector<std::string> &components);

    std::string NormalizePath(const std::string &path);

    std::string GetOptionsConfigPath(const std::string &avdName);

    std::string EnsureOptionsConfigDirectoryExists();

    namespace Onboarding {
        bool IsFirstRunComplete();

        void MarkFirstRunComplete();

        bool ValidateSdkPath(const std::string &path);

        std::string LoadSdkPathOverride();

        void SaveSdkPathOverride(const std::string &path);

        void ClearSdkPathOverride();

        std::string LoadJdkPathOverride();

        void SaveJdkPathOverride(const std::string &path);

        void ClearJdkPathOverride();
    }
}

#endif // COREDECK_PATHS_H

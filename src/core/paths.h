//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#ifndef EMU_LAUNCHER_PATHS_H
#define EMU_LAUNCHER_PATHS_H

#include <string>
#include <vector>

namespace CoreDeck::Paths {
    enum class Platform {
        Windows,
        macOS,
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

    std::string JoinPaths(const std::vector<std::string> &components);

    std::string NormalizePath(const std::string &path);

    std::string GetOptionsConfigPath(const std::string &avdName);

    std::string EnsureOptionsConfigDirectoryExists();
}

#endif //EMU_LAUNCHER_PATHS_H

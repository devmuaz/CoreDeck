//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#ifndef COREDECK_APP_PATHS_H
#define COREDECK_APP_PATHS_H

#include <string>

namespace CoreDeck::App {
    constexpr const char *APP_CONFIG_DIR = "coredeck";

    std::string GetAppConfigPath(const std::string &subPath = "");

    std::string GetResourcesDirectory();

    std::string GetAssetsDirectory();

    std::string GetOptionsConfigPath(const std::string &avdName);

    std::string EnsureOptionsConfigDirectoryExists();
}

#endif // COREDECK_APP_PATHS_H

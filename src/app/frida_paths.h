//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_APP_FRIDA_PATHS_H
#define COREDECK_APP_FRIDA_PATHS_H

#include <string>

namespace CoreDeck::App {
    std::string FridaServerCacheDirectory(const std::string &version);

    std::string FridaScriptsDirectory();

    std::string EnsureFridaProject(std::string &error);

    std::string ResetFridaProject(std::string &error);
}

#endif // COREDECK_APP_FRIDA_PATHS_H

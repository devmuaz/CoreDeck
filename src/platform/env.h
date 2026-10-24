//
// Created by AbdulMuaz Aqeel on 24/07/2026.
//

#ifndef COREDECK_ENV_H
#define COREDECK_ENV_H

#include <string>
#include <vector>

namespace CoreDeck::Platform {
    struct EnvVar {
        std::string Name;
        std::string Value;
    };

    using EnvVars = std::vector<EnvVar>;

    constexpr const char *ENV_HOME = "HOME";
    constexpr const char *ENV_USERPROFILE = "USERPROFILE";
    constexpr const char *ENV_HOMEDRIVE = "HOMEDRIVE";
    constexpr const char *ENV_HOMEPATH = "HOMEPATH";
    constexpr const char *ENV_APPDATA = "APPDATA";
    constexpr const char *ENV_LOCALAPPDATA = "LOCALAPPDATA";
    constexpr const char *ENV_PROGRAM_FILES = "ProgramFiles";
    constexpr const char *ENV_PATH = "PATH";
    constexpr const char *ENV_XDG_CONFIG_HOME = "XDG_CONFIG_HOME";

    constexpr const char *ENV_LANG = "LANG";
    constexpr const char *ENV_LANGUAGE = "LANGUAGE";
    constexpr const char *ENV_LC_ALL = "LC_ALL";
    constexpr const char *ENV_LC_MESSAGES = "LC_MESSAGES";
}


#endif // COREDECK_ENV_H

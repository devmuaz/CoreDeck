//
// Created by AbdulMuaz Aqeel on 30/09/2026.
//

#ifndef COREDECK_CONSTANTS_H
#define COREDECK_CONSTANTS_H

#include <cstdint>

namespace CoreDeck::Core {
    // JDK
    constexpr int JDK_MINIMUM_MAJOR = 17;
    constexpr int JDK_RECOMMENDED_MAJOR = 21;
    constexpr const char *JAVA_JDK21_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java21";

    // Keystore
    constexpr int KEYSTORE_PASSWORD_MIN = 6;
    constexpr int KEYSTORE_COUNTRY_CODE_LENGTH = 2;
    constexpr int KEYSTORE_VALIDITY_MIN_DAYS = 1;
    constexpr int KEYSTORE_VALIDITY_MAX_DAYS = 365000;
    constexpr int DEFAULT_KEY_VALIDITY_DAYS = 3650;
    constexpr int DEFAULT_KEY_SIZE = 2048;
    constexpr int DEFAULT_EC_KEY_SIZE = 256;

    // Android SDK
    constexpr const char *SDK_LICENSE_URL = "https://developer.android.com/studio/terms";
    constexpr const char *ANDROID_REPOSITORY_URL = "https://dl.google.com/android/repository/";
    constexpr const char *CMDLINE_TOOLS_VERSION = "15859902";
    constexpr const char *BOOTSTRAP_BUILD_TOOLS_PACKAGE = "build-tools;37.0.0";
    constexpr const char *SDK_MANAGER_PROGRESS_COLUMNS = "16384";
    constexpr std::uint64_t BOOTSTRAP_REQUIRED_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr std::uint64_t BOOTSTRAP_TOOLS_REQUIRED_BYTES = 512ULL * 1024ULL * 1024ULL;

    // CoreDeck
    constexpr const char *WEBSITE_URL = "https://coredeck.dev";
    constexpr const char *AUTHOR_WEBSITE_URL = "https://devmuaz.com";
    constexpr const char *GITHUB_URL = "https://github.com/devmuaz/CoreDeck";
    constexpr const char *GITHUB_LATEST_RELEASE_URL = "https://api.github.com/repos/devmuaz/CoreDeck/releases/latest";
    constexpr const char *USER_AGENT_PREFIX = "CoreDeck/";

    // APK Analyzer
    constexpr int RECENT_APK_LIMIT = 4;

    // Environment variables
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

    constexpr const char *ENV_JAVA_HOME = "JAVA_HOME";
    constexpr const char *ENV_SKIP_JDK_VERSION_CHECK = "SKIP_JDK_VERSION_CHECK";

    constexpr const char *ENV_ANDROID_HOME = "ANDROID_HOME";
    constexpr const char *ENV_ANDROID_SDK_ROOT = "ANDROID_SDK_ROOT";
    constexpr const char *ENV_COLUMNS = "COLUMNS";

    constexpr const char *ENV_COREDECK_ASSETS_DIR = "COREDECK_ASSETS_DIR";
    constexpr const char *ENV_COREDECK_CMDLINE_TOOLS_URL = "COREDECK_CMDLINE_TOOLS_URL";
    constexpr const char *ENV_COREDECK_CMDLINE_TOOLS_SHA256 = "COREDECK_CMDLINE_TOOLS_SHA256";
}

#endif // COREDECK_CONSTANTS_H

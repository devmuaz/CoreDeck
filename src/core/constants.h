//
// Created by AbdulMuaz Aqeel on 30/09/2026.
//

#ifndef COREDECK_CONSTANTS_H
#define COREDECK_CONSTANTS_H

#include <cstdint>

namespace CoreDeck {
    // JDK
    constexpr int JDK_MINIMUM_MAJOR = 17;
    constexpr int JDK_RECOMMENDED_MAJOR = 21;
    constexpr const char *JAVA_JDK21_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java21";

    // Android SDK
    constexpr const char *SDK_LICENSE_URL = "https://developer.android.com/studio/terms";
    constexpr const char *ANDROID_REPOSITORY_URL = "https://dl.google.com/android/repository/";
    constexpr const char *CMDLINE_TOOLS_VERSION = "15859902";
    constexpr const char *BOOTSTRAP_BUILD_TOOLS_PACKAGE = "build-tools;37.0.0";
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
}

#endif // COREDECK_CONSTANTS_H

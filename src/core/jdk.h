//
// Created by AbdulMuaz Aqeel on 24/07/2026.
//

#ifndef COREDECK_JDK_H
#define COREDECK_JDK_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "env.h"
#include "sdk.h"

namespace CoreDeck::Core {
    constexpr int JDK_MINIMUM_MAJOR = 17;
    constexpr int JDK_RECOMMENDED_MAJOR = 21;
    constexpr const char *JAVA_JDK21_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java21";
    constexpr const char *ENV_JAVA_HOME = "JAVA_HOME";
    constexpr const char *ENV_SKIP_JDK_VERSION_CHECK = "SKIP_JDK_VERSION_CHECK";

    enum class JdkSource : uint8_t {
        None,
        Override,
        JavaHomeEnv,
        Detected,
    };

    struct JdkInfo {
        std::string JavaHome;
        std::string JavaBin;
        std::string VersionString;
        int MajorVersion = 0;
        bool IsFound = false;
        bool IsValid = false;
        JdkSource Source = JdkSource::None;
    };

    JdkInfo DetectJdk(std::string_view savedHome = {});

    // Every JDK found on this machine, newest usable install first.
    // Homes that point at the same directory are listed once.
    std::vector<JdkInfo> ListInstalledJdks(std::string_view savedHome = {});

    JdkInfo InspectJdk(const std::string &javaHome);

    bool ShouldApplyJdk(const JdkInfo &jdk);

    Platform::EnvVars JavaToolEnvironment(const JdkInfo &jdk);

    std::string JdkExecutablePath(const JdkInfo &jdk, const std::string &tool);

    void ApplyJdkToSdk(SdkInfo &sdk, const JdkInfo &jdk);
}

#endif // COREDECK_JDK_H

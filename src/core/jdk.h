//
// Created by AbdulMuaz Aqeel on 24/07/2026.
//

#ifndef COREDECK_JDK_H
#define COREDECK_JDK_H

#include <cstdint>
#include <string>

#include "env.h"
#include "sdk.h"

namespace CoreDeck {
    constexpr int JDK_MINIMUM_MAJOR = 17;

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

    JdkInfo DetectJdk();

    JdkInfo InspectJdk(const std::string &javaHome);

    bool ShouldApplyJdk(const JdkInfo &jdk);

    EnvVars JavaToolEnvironment(const JdkInfo &jdk);

    void ApplyJdkToSdk(SdkInfo &sdk, const JdkInfo &jdk);
}

#endif // COREDECK_JDK_H

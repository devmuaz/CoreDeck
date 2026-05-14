//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_VERSION_CHECK_H
#define COREDECK_VERSION_CHECK_H

#include <optional>
#include <string>

namespace CoreDeck {
    struct RemoteRelease {
        std::string Version;
        std::string Notes;
    };

    std::optional<RemoteRelease> QueryRemoteNewerVersion();

    namespace detail { // NOLINT(readability-identifier-naming)
        int CompareSemanticVersion(const std::string &newVersion, const std::string &currentVersion);

        std::optional<std::string> ParseLatestReleaseTag(const std::string &body);

        std::optional<RemoteRelease> ParseLatestRelease(const std::string &body);
    }
}

#endif // COREDECK_VERSION_CHECK_H

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <rfl/json.hpp>

#include "version_check.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    struct GitHubLatestRelease {
        std::string tag_name;
    };

    static void TrimInPlace(std::string &s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
            s.erase(s.begin());
        }
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    }

    namespace detail {
        std::optional<std::string> ParseLatestReleaseTag(const std::string &body) {
            try {
                const auto parsed = rfl::json::read<GitHubLatestRelease>(body);
                if (!parsed) {
                    return std::nullopt;
                }
                const std::string &tag = parsed.value().tag_name;
                if (tag.empty()) {
                    return std::nullopt;
                }
                return tag;
            } catch (...) {
                return std::nullopt;
            }
        }

        int CompareSemanticVersion(const std::string &newVersion, const std::string &currentVersion) {
            auto parse = [](const std::string &raw) -> std::pair<std::vector<int>, bool> {
                std::string s = raw;
                if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
                    s.erase(s.begin());
                }
                const bool hasPreRelease = s.find('-') != std::string::npos;
                std::vector<int> parts;
                size_t pos = 0;
                while (pos < s.size()) {
                    const size_t dot = s.find('.', pos);
                    const std::string seg = dot == std::string::npos ? s.substr(pos) : s.substr(pos, dot - pos);
                    int n = 0;
                    for (const char c: seg) {
                        if (c < '0' || c > '9') {
                            break;
                        }
                        n = n * 10 + (c - '0');
                    }
                    parts.push_back(n);
                    if (dot == std::string::npos) {
                        break;
                    }
                    pos = dot + 1;
                }
                while (parts.size() < 3) {
                    parts.push_back(0);
                }
                return {parts, hasPreRelease};
            };

            const auto [va, preA] = parse(newVersion);
            const auto [vb, preB] = parse(currentVersion);
            const size_t n = std::max(va.size(), vb.size());
            for (size_t i = 0; i < n; ++i) {
                const int a = i < va.size() ? va[i] : 0;
                const int b = i < vb.size() ? vb[i] : 0;
                if (a != b) {
                    return a < b ? -1 : 1;
                }
            }
            if (preA && !preB) return -1;
            if (!preA && preB) return 1;
            return 0;
        }
    }

    std::optional<std::string> QueryRemoteNewerVersion() {
#if defined(_WIN32)
        const std::string cmd = StrConcat(
            "curl -sfL --max-time 15 -H \"Accept: application/vnd.github+json\" -A \"CoreDeck/",
            COREDECK_VERSION,
            "\" \"",
            COREDECK_GITHUB_API,
            "\""
        );
#else
        const std::string cmd = StrConcat(
            "curl -sfL --max-time 15 -H 'Accept: application/vnd.github+json' -A 'CoreDeck/",
            COREDECK_VERSION,
            "' '",
            COREDECK_GITHUB_API,
            "'"
        );
#endif
        std::string body = RunCommand(cmd);
        TrimInPlace(body);
        if (body.empty()) {
            return std::nullopt;
        }

        auto remote = detail::ParseLatestReleaseTag(body);
        if (!remote) {
            return std::nullopt;
        }

        if (detail::CompareSemanticVersion(remote.value(), COREDECK_VERSION) <= 0) {
            return std::nullopt;
        }
        return remote;
    }
}

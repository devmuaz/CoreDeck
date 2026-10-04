//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <rfl/json.hpp>

#include "version_check.h"
#include "constants.h"
#include "http_download.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        struct GitHubLatestRelease {
            std::string tag_name; // NOLINT(readability-identifier-naming)
            std::optional<std::string> body; // NOLINT(readability-identifier-naming)
        };

        void TrimInPlace(std::string &s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                s.erase(s.begin());
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                s.pop_back();
            }
        }

        struct ParsedVersion {
            std::vector<int> Core;
            std::vector<std::string> PreRelease;
        };

        bool IsDigits(const std::string &text) {
            return !text.empty() && std::all_of(text.begin(), text.end(), [](const unsigned char c) {
                return c >= '0' && c <= '9';
            });
        }

        // Numeric identifiers compare by value, so beta.10 is newer than beta.2.
        int ComparePreReleaseId(const std::string &left, const std::string &right) {
            const bool leftNumeric = IsDigits(left);
            const bool rightNumeric = IsDigits(right);
            if (leftNumeric && rightNumeric) {
                size_t leftIndex = 0;
                size_t rightIndex = 0;
                while (leftIndex < left.size() && left.at(leftIndex) == '0') {
                    ++leftIndex;
                }
                while (rightIndex < right.size() && right.at(rightIndex) == '0') {
                    ++rightIndex;
                }
                const size_t leftLength = left.size() - leftIndex;
                const size_t rightLength = right.size() - rightIndex;
                if (leftLength != rightLength) {
                    return leftLength < rightLength ? -1 : 1;
                }
                const int compared = left.compare(leftIndex, leftLength, right, rightIndex, rightLength);
                if (compared < 0) {
                    return -1;
                }
                if (compared > 0) {
                    return 1;
                }
                return 0;
            }
            if (leftNumeric != rightNumeric) {
                return leftNumeric ? -1 : 1;
            }
            if (left < right) {
                return -1;
            }
            if (left > right) {
                return 1;
            }
            return 0;
        }

        ParsedVersion ParseSemanticVersion(const std::string &raw) {
            std::string text = raw;
            if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
                text.erase(text.begin());
            }
            const size_t build = text.find('+');
            if (build != std::string::npos) {
                text.erase(build);
            }

            const size_t dash = text.find('-');
            const std::string coreText = dash == std::string::npos ? text : text.substr(0, dash);
            const std::string preText = dash == std::string::npos ? std::string{} : text.substr(dash + 1);

            ParsedVersion version;
            size_t pos = 0;
            while (pos < coreText.size()) {
                const size_t dot = coreText.find('.', pos);
                const std::string segment = dot == std::string::npos ? coreText.substr(pos) : coreText.substr(pos, dot - pos);
                int value = 0;
                for (const char c: segment) {
                    if (c < '0' || c > '9') {
                        break;
                    }
                    value = (value * 10) + (c - '0');
                }
                version.Core.push_back(value);
                if (dot == std::string::npos) {
                    break;
                }
                pos = dot + 1;
            }
            while (version.Core.size() < 3) {
                version.Core.push_back(0);
            }

            pos = 0;
            while (pos < preText.size()) {
                const size_t dot = preText.find('.', pos);
                version.PreRelease.push_back(dot == std::string::npos ? preText.substr(pos) : preText.substr(pos, dot - pos));
                if (dot == std::string::npos) {
                    break;
                }
                pos = dot + 1;
            }
            return version;
        }

        int CompareParsedVersion(const ParsedVersion &left, const ParsedVersion &right) {
            const size_t coreCount = std::max(left.Core.size(), right.Core.size());
            for (size_t i = 0; i < coreCount; ++i) {
                const int a = i < left.Core.size() ? left.Core.at(i) : 0;
                const int b = i < right.Core.size() ? right.Core.at(i) : 0;
                if (a != b) {
                    return a < b ? -1 : 1;
                }
            }
            if (left.PreRelease.empty() && right.PreRelease.empty()) {
                return 0;
            }
            if (left.PreRelease.empty()) {
                return 1;
            }
            if (right.PreRelease.empty()) {
                return -1;
            }

            const size_t preCount = std::max(left.PreRelease.size(), right.PreRelease.size());
            for (size_t i = 0; i < preCount; ++i) {
                if (i >= left.PreRelease.size()) {
                    return -1;
                }
                if (i >= right.PreRelease.size()) {
                    return 1;
                }
                const int compared = ComparePreReleaseId(left.PreRelease.at(i), right.PreRelease.at(i));
                if (compared != 0) {
                    return compared;
                }
            }
            return 0;
        }

        std::string ExtractWhatsNewSection(const std::string &body) {
            size_t lastSeparatorStart = std::string::npos;
            size_t scan = 0;
            while (scan < body.size()) {
                const size_t lineStart = scan;
                const size_t newline = body.find('\n', scan);
                const size_t lineEnd = newline == std::string::npos ? body.size() : newline;
                std::string line = body.substr(lineStart, lineEnd - lineStart);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line == "---") {
                    lastSeparatorStart = lineStart;
                }
                if (newline == std::string::npos) {
                    break;
                }
                scan = newline + 1;
            }

            if (lastSeparatorStart == std::string::npos) {
                return body;
            }
            return body.substr(0, lastSeparatorStart);
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


        std::optional<RemoteRelease> ParseLatestRelease(const std::string &body) {
            try {
                const auto parsed = rfl::json::read<GitHubLatestRelease>(body);
                if (!parsed) {
                    return std::nullopt;
                }
                const auto &value = parsed.value();
                if (value.tag_name.empty()) {
                    return std::nullopt;
                }
                RemoteRelease release;
                release.Version = value.tag_name;
                release.Notes = ExtractWhatsNewSection(value.body.value_or(""));
                TrimInPlace(release.Notes);
                return release;
            } catch (...) {
                return std::nullopt;
            }
        }

        int CompareSemanticVersion(const std::string &newVersion, const std::string &currentVersion) {
            return CompareParsedVersion(ParseSemanticVersion(newVersion), ParseSemanticVersion(currentVersion));
        }
    }

    std::optional<RemoteRelease> QueryRemoteNewerVersion() {
        const std::string userAgent = StrConcat(USER_AGENT_PREFIX, COREDECK_VERSION);
        auto fetched = HttpGetString(GITHUB_LATEST_RELEASE_URL, userAgent, "application/vnd.github+json");
        if (!fetched) {
            return std::nullopt;
        }
        std::string body = std::move(fetched.value());
        TrimInPlace(body);
        if (body.empty()) {
            return std::nullopt;
        }

        auto remote = detail::ParseLatestRelease(body);
        if (!remote) {
            return std::nullopt;
        }

        if (detail::CompareSemanticVersion(remote.value().Version, COREDECK_VERSION) <= 0) {
            return std::nullopt;
        }
        return remote;
    }
}

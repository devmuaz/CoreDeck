//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <algorithm>
#include <rfl/json.hpp>

#include "version_check.h"
#include "utilities.h"

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

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

    namespace {
#if defined(_WIN32)
        std::optional<std::string> HttpGet(const wchar_t *host, const wchar_t *path, const std::wstring &userAgent) {
            HINTERNET session = WinHttpOpen(
                userAgent.c_str(),
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (!session) return std::nullopt;

            WinHttpSetTimeouts(session, 15000, 15000, 15000, 15000);

            HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connect) {
                WinHttpCloseHandle(session);
                return std::nullopt;
            }

            HINTERNET request = WinHttpOpenRequest(
                connect, L"GET", path, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE
            );
            if (!request) {
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return std::nullopt;
            }

            const wchar_t *headers = L"Accept: application/vnd.github+json\r\n";
            BOOL sent = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                        && WinHttpReceiveResponse(request, nullptr);

            std::optional<std::string> result;
            if (sent) {
                DWORD status = 0;
                DWORD statusSize = sizeof(status);
                WinHttpQueryHeaders(request,
                                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                    WINHTTP_NO_HEADER_INDEX);
                if (status >= 200 && status < 300) {
                    std::string body;
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                        std::string chunk(available, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
                        chunk.resize(read);
                        body.append(chunk);
                    }
                    result = std::move(body);
                }
            }

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return result;
        }
#else
        size_t CurlWriteCallback(const char *ptr, const size_t size, size_t nmemb, void *userdata) {
            auto *buf = static_cast<std::string *>(userdata);
            buf->append(ptr, size * nmemb);
            return size * nmemb;
        }

        std::optional<std::string> HttpGet(const std::string &url, const std::string &userAgent) {
            CURL *curl = curl_easy_init();
            if (!curl) return std::nullopt;

            std::string body;
            curl_slist *headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

            const CURLcode rc = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (rc != CURLE_OK) return std::nullopt;
            return body;
        }
#endif
    }

    std::optional<std::string> QueryRemoteNewerVersion() {
#if defined(_WIN32)
        const std::string ua = StrConcat("CoreDeck/", COREDECK_VERSION);
        std::wstring userAgent(ua.begin(), ua.end());
        auto fetched = HttpGet(L"api.github.com", L"/repos/devmuaz/CoreDeck/releases/latest", userAgent);
#else
        const std::string userAgent = StrConcat("CoreDeck/", COREDECK_VERSION);
        auto fetched = HttpGet(COREDECK_GITHUB_API, userAgent);
#endif
        if (!fetched) {
            return std::nullopt;
        }
        std::string body = std::move(fetched.value());
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

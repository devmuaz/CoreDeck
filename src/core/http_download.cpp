//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#include <filesystem>
#include <fstream>

#include "http_download.h"
#include "utilities.h"

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

namespace CoreDeck {
    namespace detail {
        std::optional<ParsedUrl> ParseUrl(const std::string &url) {
            const size_t schemeEnd = url.find("://");
            if (schemeEnd == std::string::npos) {
                return std::nullopt;
            }

            ParsedUrl parsed;
            parsed.Scheme = LowerCopy(url.substr(0, schemeEnd));
            if (parsed.Scheme != "http" && parsed.Scheme != "https") {
                return std::nullopt;
            }
            parsed.IsSecure = parsed.Scheme == "https";

            const size_t hostStart = schemeEnd + 3;
            const size_t pathStart = url.find('/', hostStart);
            std::string authority = pathStart == std::string::npos
                                        ? url.substr(hostStart)
                                        : url.substr(hostStart, pathStart - hostStart);
            if (authority.empty()) {
                return std::nullopt;
            }

            if (const size_t colon = authority.rfind(':'); colon != std::string::npos) {
                const std::string portText = authority.substr(colon + 1);
                const auto port = static_cast<int64_t>(std::strtol(portText.c_str(), nullptr, 10));
                if (port <= 0 || port > 65535) {
                    return std::nullopt;
                }
                parsed.Port = static_cast<uint16_t>(port);
                authority = authority.substr(0, colon);
            } else {
                parsed.Port = parsed.IsSecure ? 443 : 80;
            }

            parsed.Host = authority;
            parsed.Path = pathStart == std::string::npos ? "/" : url.substr(pathStart);
            return parsed;
        }
    }

    namespace {
        constexpr int TIMEOUT_MS = 30000;

        using SinkFn = std::function<bool(const char *data, size_t size)>;

#if defined(_WIN32)
        std::wstring Widen(const std::string &value) {
            if (value.empty()) {
                return {};
            }
            const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
            std::wstring wide(static_cast<size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), wide.data(), size);
            return wide;
        }

        class WinHttpHandle {
        public:
            explicit WinHttpHandle(const HINTERNET handle = nullptr) : m_Handle(handle) {
            }

            WinHttpHandle(const WinHttpHandle &) = delete;
            WinHttpHandle(WinHttpHandle &&) = delete;
            WinHttpHandle &operator=(const WinHttpHandle &) = delete;
            WinHttpHandle &operator=(WinHttpHandle &&) = delete;

            ~WinHttpHandle() {
                if (m_Handle) {
                    WinHttpCloseHandle(m_Handle);
                }
            }

            [[nodiscard]] HINTERNET Get() const {
                return m_Handle;
            }
            explicit operator bool() const {
                return m_Handle != nullptr;
            }

        private:
            HINTERNET m_Handle;
        };

        bool HttpFetch(
            const std::string &url,
            const std::string &userAgent,
            const std::string &acceptHeader,
            const SinkFn &sink,
            const DownloadProgressFn &onProgress,
            std::string &error
        ) {
            const auto parsed = detail::ParseUrl(url);
            if (!parsed) {
                error = StrConcat("Invalid URL: ", url);
                return false;
            }

            const WinHttpHandle session(WinHttpOpen(
                Widen(userAgent).c_str(),
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            ));
            if (!session) {
                error = "Could not start an HTTP session.";
                return false;
            }

            WinHttpSetTimeouts(session.Get(), TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS);

            const WinHttpHandle connect(WinHttpConnect(session.Get(), Widen(parsed->Host).c_str(), parsed->Port, 0));
            if (!connect) {
                error = StrConcat("Could not connect to ", parsed->Host, ".");
                return false;
            }

            const WinHttpHandle request(WinHttpOpenRequest(
                connect.Get(),
                L"GET",
                Widen(parsed->Path).c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                parsed->IsSecure ? WINHTTP_FLAG_SECURE : 0U
            ));
            if (!request) {
                error = "Could not create the HTTP request.";
                return false;
            }

            std::wstring headers;
            if (!acceptHeader.empty()) {
                headers = Widen(StrConcat("Accept: ", acceptHeader, "\r\n"));
            }

            const BOOL sent = WinHttpSendRequest(
                request.Get(),
                headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                headers.empty() ? 0 : static_cast<DWORD>(-1L),
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0
            );
            if (!sent || !WinHttpReceiveResponse(request.Get(), nullptr)) {
                error = "The HTTP request failed.";
                return false;
            }

            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            WinHttpQueryHeaders(
                request.Get(),
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX
            );
            if (status < 200 || status >= 300) {
                error = StrConcat("Server returned HTTP ", std::to_string(status), ".");
                return false;
            }

            uint64_t total = 0;
            DWORD contentLength = 0;
            DWORD contentLengthSize = sizeof(contentLength);
            if (WinHttpQueryHeaders(
                    request.Get(),
                    WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &contentLength,
                    &contentLengthSize,
                    WINHTTP_NO_HEADER_INDEX
                )) {
                total = contentLength;
            }

            uint64_t received = 0;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request.Get(), &available) && available > 0) {
                std::string chunk(available, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(request.Get(), chunk.data(), available, &read)) {
                    error = "The download was interrupted.";
                    return false;
                }
                if (read == 0) {
                    break;
                }
                if (!sink(chunk.data(), read)) {
                    error = "Could not write the downloaded data.";
                    return false;
                }
                received += read;
                if (onProgress && !onProgress(received, total)) {
                    error = "Cancelled.";
                    return false;
                }
            }

            return true;
        }
#else
        struct FetchState {
            const SinkFn *Sink = nullptr;
            const DownloadProgressFn *Progress = nullptr;
            uint64_t Received = 0;
            bool SinkFailed = false;
            bool Cancelled = false;
        };

        size_t CurlWriteCallback(const char *ptr, const size_t size, const size_t nmemb, void *userdata) {
            auto *state = static_cast<FetchState *>(userdata);
            const size_t bytes = size * nmemb;
            if (!(*state->Sink)(ptr, bytes)) {
                state->SinkFailed = true;
                return 0;
            }
            state->Received += bytes;
            return bytes;
        }

        int CurlProgressCallback(
            void *userdata,
            const curl_off_t dlTotal,
            const curl_off_t dlNow,
            curl_off_t /*ulTotal*/,
            curl_off_t /*ulNow*/
        ) {
            auto *state = static_cast<FetchState *>(userdata);
            if (!state->Progress || !*state->Progress) {
                return 0;
            }
            const auto received = static_cast<uint64_t>(dlNow < 0 ? 0 : dlNow);
            const auto total = static_cast<uint64_t>(dlTotal < 0 ? 0 : dlTotal);
            if (!(*state->Progress)(received, total)) {
                state->Cancelled = true;
                return 1;
            }
            return 0;
        }

        bool HttpFetch(
            const std::string &url,
            const std::string &userAgent,
            const std::string &acceptHeader,
            const SinkFn &sink,
            const DownloadProgressFn &onProgress,
            std::string &error
        ) {
            CURL *curl = curl_easy_init();
            if (!curl) {
                error = "Could not initialize the HTTP client.";
                return false;
            }

            FetchState state;
            state.Sink = &sink;
            state.Progress = &onProgress;

            curl_slist *headers = nullptr;
            if (!acceptHeader.empty()) {
                headers = curl_slist_append(nullptr, StrConcat("Accept: ", acceptHeader).c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            if (headers) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlProgressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);

            const CURLcode rc = curl_easy_perform(curl);
            long status = 0; // NOLINT(google-runtime-int) required by the libcurl API
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

            if (headers) {
                curl_slist_free_all(headers);
            }
            curl_easy_cleanup(curl);

            if (rc == CURLE_OK) {
                return true;
            }
            if (state.Cancelled) {
                error = "Cancelled.";
            } else if (state.SinkFailed) {
                error = "Could not write the downloaded data.";
            } else if (status >= 400) {
                error = StrConcat("Server returned HTTP ", std::to_string(status), ".");
            } else {
                error = curl_easy_strerror(rc);
            }
            return false;
        }
#endif
    }

    std::optional<std::string> HttpGetString(
        const std::string &url,
        const std::string &userAgent,
        const std::string &acceptHeader
    ) {
        std::string body;
        std::string error;
        const bool ok = HttpFetch(
            url,
            userAgent,
            acceptHeader,
            [&body](const char *data, const size_t size) {
                body.append(data, size);
                return true;
            },
            nullptr,
            error
        );
        if (!ok) {
            return std::nullopt;
        }
        return body;
    }

    bool HttpDownloadToFile(
        const std::string &url,
        const std::string &destPath,
        const std::string &userAgent,
        const DownloadProgressFn &onProgress,
        std::string &error
    ) {
        const std::filesystem::path target(destPath);
        if (target.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(target.parent_path(), ec);
        }

        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = StrConcat("Could not create file: ", destPath);
            return false;
        }

        const bool ok = HttpFetch(
            url,
            userAgent,
            "",
            [&out](const char *data, const size_t size) {
                out.write(data, static_cast<std::streamsize>(size));
                return static_cast<bool>(out);
            },
            onProgress,
            error
        );

        out.close();

        if (!ok) {
            std::error_code ec;
            std::filesystem::remove(target, ec);
        }
        return ok;
    }
}

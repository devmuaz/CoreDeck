//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#ifndef COREDECK_HTTP_DOWNLOAD_H
#define COREDECK_HTTP_DOWNLOAD_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace CoreDeck {
    using DownloadProgressFn = std::function<bool(uint64_t received, uint64_t total)>;

    std::optional<std::string> HttpGetString(
        const std::string &url,
        const std::string &userAgent,
        const std::string &acceptHeader = ""
    );

    bool HttpDownloadToFile(
        const std::string &url,
        const std::string &destPath,
        const std::string &userAgent,
        const DownloadProgressFn &onProgress,
        std::string &error
    );

    namespace detail { // NOLINT(readability-identifier-naming)
        struct ParsedUrl {
            std::string Scheme;
            std::string Host;
            std::string Path;
            uint16_t Port = 0;
            bool IsSecure = true;
        };

        std::optional<ParsedUrl> ParseUrl(const std::string &url);
    }
}

#endif // COREDECK_HTTP_DOWNLOAD_H

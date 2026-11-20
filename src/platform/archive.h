//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#ifndef COREDECK_PLATFORM_ARCHIVE_H
#define COREDECK_PLATFORM_ARCHIVE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace CoreDeck::Platform {
    struct ExtractOptions {
        // Drops the first path component of every entry (e.g. "cmdline-tools/bin/x" -> "bin/x").
        bool StripTopLevelDir = false;
    };

    using ExtractProgressFn = std::function<bool(float progress)>;

    bool ExtractZip(
        const std::string &zipPath,
        const std::string &destDir,
        const ExtractOptions &options,
        const ExtractProgressFn &onProgress,
        std::string &error
    );

    bool DecompressXz(const std::string &sourcePath, const std::string &destPath, std::string &error);

    struct ZipPayload {
        std::string Name;
        std::uint64_t DataOffset = 0;
        bool Stored = false;
        bool Directory = false;
    };

    // libarchive does not report where a zip payload sits in the file. APK alignment needs that offset.
    bool ReadZipPayloads(const std::string &zipPath, std::vector<ZipPayload> &payloads, std::string &error);

    namespace detail { // NOLINT(readability-identifier-naming)
        bool IsSafeArchiveEntry(const std::string &entryName);

        std::string NormalizeArchiveEntry(const std::string &entryName);

        std::string StripLeadingComponent(const std::string &entryName);
    }

    void MakeFileExecutable(const std::string &path);
}

#endif // COREDECK_PLATFORM_ARCHIVE_H

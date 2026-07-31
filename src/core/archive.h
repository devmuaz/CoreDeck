//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#ifndef COREDECK_ARCHIVE_H
#define COREDECK_ARCHIVE_H

#include <functional>
#include <string>

namespace CoreDeck {
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

    namespace detail { // NOLINT(readability-identifier-naming)
        bool IsSafeArchiveEntry(const std::string &entryName);

        std::string NormalizeArchiveEntry(const std::string &entryName);

        std::string StripLeadingComponent(const std::string &entryName);
    }

    void MakeFileExecutable(const std::string &path);
}

#endif // COREDECK_ARCHIVE_H

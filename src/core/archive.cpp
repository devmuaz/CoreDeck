//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#include <filesystem>
#include <fstream>

#include <miniz.h>

#include "archive.h"
#include "utilities.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace CoreDeck {
    namespace detail {
        std::string NormalizeArchiveEntry(const std::string &entryName) {
            std::string normalized = entryName;
            for (char &c: normalized) {
                if (c == '\\') {
                    c = '/';
                }
            }
            while (normalized.starts_with("./")) {
                normalized.erase(0, 2);
            }
            return normalized;
        }

        bool IsSafeArchiveEntry(const std::string &entryName) {
            const std::string normalized = NormalizeArchiveEntry(entryName);
            if (normalized.empty()) {
                return false;
            }
            if (normalized.front() == '/') {
                return false;
            }
            // Drive-qualified paths such as "C:/x" or "C:x".
            if (normalized.size() >= 2 && normalized[1] == ':') {
                return false;
            }

            size_t start = 0;
            while (start <= normalized.size()) {
                const size_t slash = normalized.find('/', start);
                const std::string component = slash == std::string::npos
                                                  ? normalized.substr(start)
                                                  : normalized.substr(start, slash - start);
                if (component == "..") {
                    return false;
                }
                if (slash == std::string::npos) {
                    break;
                }
                start = slash + 1;
            }
            return true;
        }

        std::string StripLeadingComponent(const std::string &entryName) {
            const std::string normalized = NormalizeArchiveEntry(entryName);
            const size_t slash = normalized.find('/');
            if (slash == std::string::npos) {
                return {};
            }
            return normalized.substr(slash + 1);
        }
    }

    void MakeFileExecutable(const std::string &path) {
#if defined(_WIN32)
        (void) path;
#else
        struct stat info = {};
        if (stat(path.c_str(), &info) != 0) {
            return;
        }
        const mode_t mode = info.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
        (void) chmod(path.c_str(), mode);
#endif
    }

    namespace {
        struct WriteState {
            std::ofstream Out;
            bool Failed = false;
        };

        size_t WriteCallback(void *opaque, mz_uint64 /*offset*/, const void *buffer, const size_t size) {
            auto *state = static_cast<WriteState *>(opaque);
            if (state->Failed) {
                return 0;
            }
            state->Out.write(static_cast<const char *>(buffer), static_cast<std::streamsize>(size));
            if (!state->Out) {
                state->Failed = true;
                return 0;
            }
            return size;
        }

        bool ShouldBeExecutable(const std::string &relativePath, const mz_uint32 externalAttr) {
            // Unix permission bits live in the high 16 bits of the external attributes.
            if (const mz_uint32 unixMode = externalAttr >> 16; (unixMode & 0111U) != 0) {
                return true;
            }
            // Google's cmdline-tools archive is not always built with unix attributes,
            // and sdkmanager/avdmanager are unusable without the exec bit.
            return relativePath.starts_with("bin/") || relativePath.find("/bin/") != std::string::npos;
        }
    }

    bool ExtractZip(
        const std::string &zipPath,
        const std::string &destDir,
        const ExtractOptions &options,
        const ExtractProgressFn &onProgress,
        std::string &error
    ) {
        mz_zip_archive zip = {};
        if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) {
            error = StrConcat("Could not open archive: ", zipPath);
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(destDir, ec);
        if (ec) {
            mz_zip_reader_end(&zip);
            error = StrConcat("Could not create destination directory: ", destDir);
            return false;
        }

        const mz_uint entryCount = mz_zip_reader_get_num_files(&zip);
        bool ok = true;

        for (mz_uint i = 0; i < entryCount && ok; ++i) {
            mz_zip_archive_file_stat stat = {};
            if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
                error = "Corrupt archive entry.";
                ok = false;
                break;
            }

            const std::string rawName = stat.m_filename;
            if (!detail::IsSafeArchiveEntry(rawName)) {
                error = StrConcat("Refusing to extract unsafe archive entry: ", rawName);
                ok = false;
                break;
            }

            std::string relative = detail::NormalizeArchiveEntry(rawName);
            if (options.StripTopLevelDir) {
                relative = detail::StripLeadingComponent(relative);
            }
            if (relative.empty()) {
                continue;
            }

            const std::filesystem::path target = std::filesystem::path(destDir) / std::filesystem::path(relative);

            if (mz_zip_reader_is_file_a_directory(&zip, i)) {
                std::filesystem::create_directories(target, ec);
                if (ec) {
                    error = StrConcat("Could not create directory: ", target.string());
                    ok = false;
                    break;
                }
                continue;
            }

            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                error = StrConcat("Could not create directory: ", target.parent_path().string());
                ok = false;
                break;
            }

            WriteState state;
            state.Out.open(target, std::ios::binary | std::ios::trunc);
            if (!state.Out) {
                error = StrConcat("Could not write file: ", target.string());
                ok = false;
                break;
            }

            if (!mz_zip_reader_extract_to_callback(&zip, i, WriteCallback, &state, 0) || state.Failed) {
                state.Out.close();
                error = StrConcat("Could not extract file: ", relative);
                ok = false;
                break;
            }
            state.Out.close();

            if (ShouldBeExecutable(relative, stat.m_external_attr)) {
                MakeFileExecutable(target.string());
            }

            if (onProgress) {
                const float progress = entryCount == 0
                                           ? 1.0F
                                           : static_cast<float>(i + 1) / static_cast<float>(entryCount);
                if (!onProgress(progress)) {
                    error = "Extraction cancelled.";
                    ok = false;
                    break;
                }
            }
        }

        mz_zip_reader_end(&zip);
        return ok;
    }
}

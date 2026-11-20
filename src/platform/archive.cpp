//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include "platform/archive.h"
#include "platform/strings.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace CoreDeck::Platform {
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
            if (normalized.size() >= 2 && normalized.at(1) == ':') {
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
#ifdef _WIN32
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
        struct ArchiveRead {
            archive *Handle = nullptr;

            ~ArchiveRead() {
                if (Handle != nullptr) {
                    archive_read_free(Handle);
                }
            }

            ArchiveRead() = default;
            ArchiveRead(ArchiveRead &&) = delete;
            ArchiveRead &operator=(ArchiveRead &&) = delete;
            ArchiveRead(const ArchiveRead &) = delete;
            ArchiveRead &operator=(const ArchiveRead &) = delete;
        };

        std::string ArchiveMessage(archive *handle, const char *fallback) {
            if (handle == nullptr) {
                return fallback;
            }
            const char *message = archive_error_string(handle);
            if (message == nullptr || message[0] == '\0') {
                return fallback;
            }
            return message;
        }

        bool EnsureParent(const std::filesystem::path &path, std::string &error) {
            std::error_code failure;
            std::filesystem::create_directories(path.parent_path(), failure);
            if (failure) {
                error = failure.message();
                return false;
            }
            return true;
        }

        bool ShouldBeExecutable(const std::string &relativePath, const unsigned perm) {
            if ((perm & 0111U) != 0) {
                return true;
            }
            // Google's cmdline-tools archive is not always built with unix attributes,
            // and sdkmanager/avdmanager are unusable without the exec bit.
            return relativePath.starts_with("bin/") || relativePath.find("/bin/") != std::string::npos;
        }

        archive *OpenZip(const std::string &zipPath, std::string &error) {
            archive *handle = archive_read_new();
            if (handle == nullptr) {
                error = StrConcat("Could not open archive: ", zipPath);
                return nullptr;
            }
            if (archive_read_support_format_zip(handle) != ARCHIVE_OK ||
                archive_read_open_filename(handle, zipPath.c_str(), 10240) != ARCHIVE_OK) {
                error = StrConcat("Could not open archive: ", zipPath);
                archive_read_free(handle);
                return nullptr;
            }
            return handle;
        }

        bool CountZipEntries(const std::string &zipPath, std::size_t &count, std::string &error) {
            ArchiveRead reader;
            reader.Handle = OpenZip(zipPath, error);
            if (reader.Handle == nullptr) {
                return false;
            }
            count = 0;
            archive_entry *entry = nullptr;
            for (;;) {
                const int result = archive_read_next_header(reader.Handle, &entry);
                if (result == ARCHIVE_EOF) {
                    return true;
                }
                if (result != ARCHIVE_OK && result != ARCHIVE_WARN) {
                    error = ArchiveMessage(reader.Handle, "Corrupt archive entry.");
                    return false;
                }
                ++count;
            }
        }

        bool WriteArchiveData(archive *handle, std::ofstream &output, std::string &error) {
            std::vector<char> chunk(static_cast<std::size_t>(1U << 16U));
            for (;;) {
                const la_ssize_t read = archive_read_data(handle, chunk.data(), chunk.size());
                if (read == 0) {
                    return true;
                }
                if (read < 0) {
                    error = ArchiveMessage(handle, "Could not extract file.");
                    return false;
                }
                output.write(chunk.data(), static_cast<std::streamsize>(read));
                if (!output) {
                    error = "Could not extract file.";
                    return false;
                }
            }
        }

        std::uint16_t ReadLe16(const unsigned char *data) {
            return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
        }

        std::uint32_t ReadLe32(const unsigned char *data) {
            return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
                   (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
        }

        std::uint64_t ReadLe64(const unsigned char *data) {
            return static_cast<std::uint64_t>(ReadLe32(data)) | (static_cast<std::uint64_t>(ReadLe32(data + 4)) << 32);
        }

        bool ReadAt(std::ifstream &file, const std::uint64_t offset, void *data, const std::size_t size) {
            if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
                return false;
            }
            file.clear();
            file.seekg(static_cast<std::streamoff>(offset));
            file.read(static_cast<char *>(data), static_cast<std::streamsize>(size));
            return static_cast<bool>(file) && std::cmp_equal(file.gcount(), size);
        }

        constexpr std::uint32_t ZIP64_MARKER32 = 0xFFFFFFFFU;
        constexpr std::uint16_t ZIP64_MARKER16 = 0xFFFFU;
        constexpr std::uint32_t DOS_DIRECTORY_BIT = 0x10U;
        constexpr std::size_t MAX_CENTRAL_DIRECTORY_BYTES = static_cast<const std::size_t>(128U * 1024U * 1024U);

        bool Zip64LocalOffset(const unsigned char *extra, const std::size_t extraSize, const bool skipUncompressed, const bool skipCompressed, std::uint64_t &localOffset) {
            std::size_t cursor = 0;
            while (cursor + 4 <= extraSize) {
                const std::uint16_t id = ReadLe16(extra + cursor);
                const std::uint16_t fieldSize = ReadLe16(extra + cursor + 2);
                if (cursor + 4 + fieldSize > extraSize) {
                    return false;
                }
                const unsigned char *field = extra + cursor + 4;
                cursor += static_cast<std::size_t>(4 + fieldSize);
                if (id != 0x0001) {
                    continue;
                }
                // APPNOTE order: uncompressed size, compressed size, then local header offset.
                std::size_t pos = 0;
                if (skipUncompressed) {
                    pos += 8;
                }
                if (skipCompressed) {
                    pos += 8;
                }
                if (pos + 8 > fieldSize) {
                    return false;
                }
                localOffset = ReadLe64(field + pos);
                return true;
            }
            return false;
        }

        struct CentralDirectoryLocation {
            std::uint64_t Offset = 0;
            std::uint64_t Size = 0;
            std::uint64_t EntryCount = 0;
        };

        enum class EndMatch : std::uint8_t {
            Absent,
            Found,
            Corrupt,
        };

        bool ReadZip64Directory(std::ifstream &file, const std::uint64_t endOffset, CentralDirectoryLocation &location, std::string &error) {
            if (endOffset < 20) {
                error = "The zip directory is corrupt.";
                return false;
            }
            unsigned char locator[20] = {};
            if (!ReadAt(file, endOffset - 20, locator, sizeof(locator)) || ReadLe32(locator) != 0x07064b50U) {
                error = "The zip directory is corrupt.";
                return false;
            }
            const std::uint64_t zip64Offset = ReadLe64(locator + 8);
            unsigned char record[56] = {};
            if (!ReadAt(file, zip64Offset, record, sizeof(record)) || ReadLe32(record) != 0x06064b50U) {
                error = "The zip directory is corrupt.";
                return false;
            }
            if (ReadLe32(record + 16) != 0 || ReadLe32(record + 20) != 0) {
                error = "The zip directory is corrupt.";
                return false;
            }
            location.EntryCount = ReadLe64(record + 32);
            location.Size = ReadLe64(record + 40);
            location.Offset = ReadLe64(record + 48);
            return true;
        }

        EndMatch TryEndOfCentralDirectory(
            const std::vector<unsigned char> &tail,
            const std::size_t index,
            std::ifstream &file,
            const std::uint64_t endOffset,
            CentralDirectoryLocation &location,
            std::string &error
        ) {
            if (tail.at(index) != 0x50 || tail.at(index + 1) != 0x4b || tail.at(index + 2) != 0x05 || tail.at(index + 3) != 0x06) {
                return EndMatch::Absent;
            }
            const std::uint16_t commentLength = ReadLe16(tail.data() + index + 20);
            if (index + 22 + commentLength != tail.size()) {
                return EndMatch::Absent;
            }

            const std::uint16_t disk = ReadLe16(tail.data() + index + 4);
            const std::uint16_t directoryDisk = ReadLe16(tail.data() + index + 6);
            const std::uint16_t entriesOnDisk = ReadLe16(tail.data() + index + 8);
            const std::uint16_t entries = ReadLe16(tail.data() + index + 10);
            const std::uint32_t size32 = ReadLe32(tail.data() + index + 12);
            const std::uint32_t offset32 = ReadLe32(tail.data() + index + 16);
            const bool zip64 = disk == ZIP64_MARKER16 || directoryDisk == ZIP64_MARKER16 || entriesOnDisk == ZIP64_MARKER16 || entries == ZIP64_MARKER16 ||
                               size32 == ZIP64_MARKER32 || offset32 == ZIP64_MARKER32;
            if (!zip64) {
                if (disk != 0 || directoryDisk != 0) {
                    error = "The zip directory is corrupt.";
                    return EndMatch::Corrupt;
                }
                location.Offset = offset32;
                location.Size = size32;
                location.EntryCount = entries;
                return EndMatch::Found;
            }
            return ReadZip64Directory(file, endOffset, location, error) ? EndMatch::Found : EndMatch::Corrupt;
        }

        bool FindCentralDirectory(
            std::ifstream &file,
            const std::uint64_t fileSize,
            const std::vector<unsigned char> &tail,
            const std::uint64_t tailSize,
            CentralDirectoryLocation &location,
            std::string &error
        ) {
            if (tail.size() < 22) {
                error = "Could not read the zip directory.";
                return false;
            }
            for (std::size_t index = tail.size() - 22;; --index) {
                const EndMatch match = TryEndOfCentralDirectory(tail, index, file, fileSize - tailSize + index, location, error);
                if (match == EndMatch::Corrupt) {
                    return false;
                }
                if (match == EndMatch::Found) {
                    return true;
                }
                if (index == 0) {
                    break;
                }
            }
            error = "Could not read the zip directory.";
            return false;
        }

        bool ReadCentralEntry(
            std::ifstream &file,
            const std::uint64_t fileSize,
            const unsigned char *directory,
            const std::size_t directorySize,
            std::size_t &cursor,
            std::vector<ZipPayload> &payloads,
            std::string &error
        ) {
            if (cursor + 46 > directorySize || ReadLe32(directory + cursor) != 0x02014b50U) {
                error = "The zip directory is corrupt.";
                return false;
            }
            const unsigned char *header = directory + cursor;
            const std::uint16_t method = ReadLe16(header + 10);
            const std::uint32_t compressedSize = ReadLe32(header + 20);
            const std::uint32_t uncompressedSize = ReadLe32(header + 24);
            const std::uint16_t nameLength = ReadLe16(header + 28);
            const std::uint16_t extraLength = ReadLe16(header + 30);
            const std::uint16_t commentLength = ReadLe16(header + 32);
            const std::uint32_t externalAttr = ReadLe32(header + 38);
            std::uint64_t localOffset = ReadLe32(header + 42);
            const std::size_t recordSize = static_cast<std::size_t>(46) + nameLength + extraLength + commentLength;
            if (cursor + recordSize > directorySize) {
                error = "The zip directory is corrupt.";
                return false;
            }
            if (localOffset == ZIP64_MARKER32) {
                const unsigned char *extra = header + 46 + nameLength;
                if (!Zip64LocalOffset(extra, extraLength, uncompressedSize == ZIP64_MARKER32, compressedSize == ZIP64_MARKER32, localOffset)) {
                    error = "The zip directory is corrupt.";
                    return false;
                }
            }

            ZipPayload payload;
            payload.Name.assign(reinterpret_cast<const char *>(header + 46), nameLength);
            payload.Stored = method == 0;
            payload.Directory = (!payload.Name.empty() && payload.Name.back() == '/') || (externalAttr & DOS_DIRECTORY_BIT) != 0;
            if (payload.Directory) {
                payloads.push_back(std::move(payload));
            } else {
                unsigned char local[30] = {};
                if (ReadAt(file, localOffset, local, sizeof(local)) && ReadLe32(local) == 0x04034b50U) {
                    const std::uint64_t dataOffset = localOffset + 30U + ReadLe16(local + 26) + ReadLe16(local + 28);
                    if (dataOffset >= localOffset && dataOffset <= fileSize) {
                        payload.DataOffset = dataOffset;
                        payloads.push_back(std::move(payload));
                    }
                }
            }
            cursor += recordSize;
            return true;
        }

        bool ReadCentralDirectory(const std::string &zipPath, std::vector<ZipPayload> &payloads, std::string &error) {
            std::error_code failure;
            const auto fileSizeRaw = std::filesystem::file_size(zipPath, failure);
            if (failure) {
                error = StrConcat("Could not open archive: ", zipPath);
                return false;
            }
            const auto fileSize = static_cast<std::uint64_t>(fileSizeRaw);
            std::ifstream file(zipPath, std::ios::binary);
            if (!file) {
                error = StrConcat("Could not open archive: ", zipPath);
                return false;
            }

            constexpr std::uint64_t TAIL_LIMIT = 22U + 65535U + 20U;
            const std::uint64_t tailSize = fileSize < TAIL_LIMIT ? fileSize : TAIL_LIMIT;
            std::vector<unsigned char> tail(static_cast<std::size_t>(tailSize));
            if (!tail.empty() && !ReadAt(file, fileSize - tailSize, tail.data(), tail.size())) {
                error = "Could not read the zip directory.";
                return false;
            }

            CentralDirectoryLocation location;
            if (!FindCentralDirectory(file, fileSize, tail, tailSize, location, error)) {
                return false;
            }
            if (location.EntryCount == 0) {
                payloads.clear();
                return true;
            }
            if (location.Size > MAX_CENTRAL_DIRECTORY_BYTES || location.Offset > fileSize || location.Size > fileSize - location.Offset) {
                error = "The zip directory is corrupt.";
                return false;
            }

            std::vector<unsigned char> directory(static_cast<std::size_t>(location.Size));
            if (!ReadAt(file, location.Offset, directory.data(), directory.size())) {
                error = "Could not read the zip directory.";
                return false;
            }

            payloads.clear();
            payloads.reserve(static_cast<std::size_t>(location.EntryCount));
            std::size_t cursor = 0;
            for (std::uint64_t seen = 0; seen < location.EntryCount; ++seen) {
                if (!ReadCentralEntry(file, fileSize, directory.data(), directory.size(), cursor, payloads, error)) {
                    return false;
                }
            }
            return true;
        }
    }

    bool ExtractZip(
        const std::string &zipPath,
        const std::string &destDir,
        const ExtractOptions &options,
        const ExtractProgressFn &onProgress,
        std::string &error
    ) {
        std::size_t entryCount = 0;
        if (!CountZipEntries(zipPath, entryCount, error)) {
            return false;
        }

        ArchiveRead reader;
        reader.Handle = OpenZip(zipPath, error);
        if (reader.Handle == nullptr) {
            return false;
        }

        std::error_code failure;
        std::filesystem::create_directories(destDir, failure);
        if (failure) {
            error = StrConcat("Could not create destination directory: ", destDir);
            return false;
        }

        std::size_t index = 0;
        archive_entry *entry = nullptr;
        while (index < entryCount) {
            const int result = archive_read_next_header(reader.Handle, &entry);
            if (result == ARCHIVE_EOF) {
                break;
            }
            if (result != ARCHIVE_OK && result != ARCHIVE_WARN) {
                error = ArchiveMessage(reader.Handle, "Corrupt archive entry.");
                return false;
            }
            ++index;

            const char *rawPath = archive_entry_pathname(entry);
            const std::string rawName = rawPath == nullptr ? std::string() : rawPath;
            if (!detail::IsSafeArchiveEntry(rawName)) {
                error = StrConcat("Refusing to extract unsafe archive entry: ", rawName);
                return false;
            }

            std::string relative = detail::NormalizeArchiveEntry(rawName);
            if (options.StripTopLevelDir) {
                relative = detail::StripLeadingComponent(relative);
            }
            if (relative.empty()) {
                continue;
            }

            const std::filesystem::path target = std::filesystem::path(destDir) / std::filesystem::path(relative);
            const auto fileType = archive_entry_filetype(entry);
            if (fileType == AE_IFDIR || relative.back() == '/') {
                std::filesystem::create_directories(target, failure);
                if (failure) {
                    error = StrConcat("Could not create directory: ", target.string());
                    return false;
                }
                continue;
            }

            std::filesystem::create_directories(target.parent_path(), failure);
            if (failure) {
                error = StrConcat("Could not create directory: ", target.parent_path().string());
                return false;
            }

            std::ofstream output(target, std::ios::binary | std::ios::trunc);
            if (!output) {
                error = StrConcat("Could not write file: ", target.string());
                return false;
            }
            std::string writeError;
            if (!WriteArchiveData(reader.Handle, output, writeError)) {
                output.close();
                error = StrConcat("Could not extract file: ", relative);
                return false;
            }
            output.close();
            if (!output) {
                error = StrConcat("Could not extract file: ", relative);
                return false;
            }

            if (ShouldBeExecutable(relative, archive_entry_perm(entry))) {
                MakeFileExecutable(target.string());
            }

            if (onProgress) {
                const float progress = entryCount == 0 ? 1.0F : static_cast<float>(index) / static_cast<float>(entryCount);
                if (!onProgress(progress)) {
                    error = "Extraction cancelled.";
                    return false;
                }
            }
        }
        return true;
    }

    bool DecompressXz(const std::string &sourcePath, const std::string &destPath, std::string &error) {
        std::error_code failure;
        const auto fileSize = std::filesystem::file_size(sourcePath, failure);
        if (failure) {
            error = "Could not read the xz archive.";
            return false;
        }
        if (fileSize == 0) {
            error = "The xz archive is empty.";
            return false;
        }
        if (!EnsureParent(destPath, error)) {
            return false;
        }

        ArchiveRead reader;
        reader.Handle = archive_read_new();
        if (reader.Handle == nullptr || archive_read_support_filter_xz(reader.Handle) != ARCHIVE_OK ||
            archive_read_support_format_raw(reader.Handle) != ARCHIVE_OK ||
            archive_read_open_filename(reader.Handle, sourcePath.c_str(), 10240) != ARCHIVE_OK) {
            error = "Could not start the xz decoder.";
            return false;
        }

        archive_entry *entry = nullptr;
        const int header = archive_read_next_header(reader.Handle, &entry);
        if (header != ARCHIVE_OK && header != ARCHIVE_WARN) {
            error = "The xz archive could not be decompressed.";
            return false;
        }

        std::ofstream output(destPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Could not write the decompressed server.";
            return false;
        }
        std::string writeError;
        const bool wrote = WriteArchiveData(reader.Handle, output, writeError);
        output.close();
        if (!wrote || !output) {
            std::filesystem::remove(destPath, failure);
            error = "The xz archive could not be decompressed.";
            return false;
        }
        return true;
    }

    bool ReadZipPayloads(const std::string &zipPath, std::vector<ZipPayload> &payloads, std::string &error) {
        return ReadCentralDirectory(zipPath, payloads, error);
    }
}

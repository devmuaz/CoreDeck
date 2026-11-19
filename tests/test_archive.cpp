#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include "platform/archive.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck;

namespace {
    using ZipEntry = std::pair<std::string, std::string>;

    std::filesystem::path MakeScratchDir(const std::string &name) {
        const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("coredeck_archive_" + name);
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    bool WriteZip(const std::filesystem::path &zipPath, const std::vector<ZipEntry> &entries) {
        struct archive *zip = archive_write_new();
        if (zip == nullptr) {
            return false;
        }
        bool ok = archive_write_set_format_zip(zip) == ARCHIVE_OK &&
                  archive_write_open_filename(zip, zipPath.string().c_str()) == ARCHIVE_OK;
        for (const auto &[name, content]: entries) {
            if (!ok) {
                break;
            }
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, name.c_str());
            archive_entry_set_size(entry, static_cast<la_int64_t>(content.size()));
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            if (archive_write_header(zip, entry) != ARCHIVE_OK) {
                ok = false;
            } else if (!content.empty()) {
                const la_ssize_t written = archive_write_data(zip, content.data(), content.size());
                ok = written >= 0 && static_cast<std::size_t>(written) == content.size();
            }
            archive_entry_free(entry);
        }
        ok = archive_write_close(zip) == ARCHIVE_OK && ok;
        archive_write_free(zip);
        return ok;
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator(in), std::istreambuf_iterator<char>()};
    }

    void Append16(std::vector<unsigned char> &out, const std::uint16_t value) {
        out.push_back(static_cast<unsigned char>(value & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    }

    void Append32(std::vector<unsigned char> &out, const std::uint32_t value) {
        out.push_back(static_cast<unsigned char>(value & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
        out.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
    }

    void Append64(std::vector<unsigned char> &out, const std::uint64_t value) {
        Append32(out, static_cast<std::uint32_t>(value));
        Append32(out, static_cast<std::uint32_t>(value >> 32));
    }

    void AppendLocal(std::vector<unsigned char> &out, const std::string &name, const std::string &content) {
        Append32(out, 0x04034b50U);
        Append16(out, 20);
        Append16(out, 0);
        Append16(out, 0);
        Append16(out, 0);
        Append16(out, 0);
        Append32(out, 0);
        Append32(out, static_cast<std::uint32_t>(content.size()));
        Append32(out, static_cast<std::uint32_t>(content.size()));
        Append16(out, static_cast<std::uint16_t>(name.size()));
        Append16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), content.begin(), content.end());
    }

    std::vector<unsigned char> StoredZip(const std::vector<ZipEntry> &entries, const bool zip64Offset) {
        std::vector<unsigned char> zip;
        std::vector<std::uint64_t> offsets;
        offsets.reserve(entries.size());
        for (const auto &[name, content]: entries) {
            offsets.push_back(zip.size());
            AppendLocal(zip, name, content);
        }
        const std::uint64_t directoryOffset = zip.size();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const std::string &name = entries.at(index).first;
            const std::string &content = entries.at(index).second;
            Append32(zip, 0x02014b50U);
            Append16(zip, 20);
            Append16(zip, 20);
            Append16(zip, 0);
            Append16(zip, 0);
            Append16(zip, 0);
            Append16(zip, 0);
            Append32(zip, 0);
            Append32(zip, static_cast<std::uint32_t>(content.size()));
            Append32(zip, static_cast<std::uint32_t>(content.size()));
            Append16(zip, static_cast<std::uint16_t>(name.size()));
            const std::uint16_t extraLength = zip64Offset ? 12 : 0;
            Append16(zip, extraLength);
            Append16(zip, 0);
            Append16(zip, 0);
            Append16(zip, 0);
            Append32(zip, 0);
            Append32(zip, zip64Offset ? 0xFFFFFFFFU : static_cast<std::uint32_t>(offsets.at(index)));
            zip.insert(zip.end(), name.begin(), name.end());
            if (zip64Offset) {
                Append16(zip, 0x0001);
                Append16(zip, 8);
                Append64(zip, offsets.at(index));
            }
        }
        const std::uint64_t directorySize = zip.size() - directoryOffset;
        if (zip64Offset) {
            const std::uint64_t recordOffset = zip.size();
            Append32(zip, 0x06064b50U);
            Append64(zip, 44);
            Append16(zip, 45);
            Append16(zip, 45);
            Append32(zip, 0);
            Append32(zip, 0);
            Append64(zip, entries.size());
            Append64(zip, entries.size());
            Append64(zip, directorySize);
            Append64(zip, directoryOffset);
            Append32(zip, 0x07064b50U);
            Append32(zip, 0);
            Append64(zip, recordOffset);
            Append32(zip, 1);
            Append32(zip, 0x06054b50U);
            Append16(zip, 0);
            Append16(zip, 0);
            Append16(zip, 0xFFFF);
            Append16(zip, 0xFFFF);
            Append32(zip, 0xFFFFFFFFU);
            Append32(zip, 0xFFFFFFFFU);
            Append16(zip, 0);
        } else {
            Append32(zip, 0x06054b50U);
            Append16(zip, 0);
            Append16(zip, 0);
            Append16(zip, static_cast<std::uint16_t>(entries.size()));
            Append16(zip, static_cast<std::uint16_t>(entries.size()));
            Append32(zip, static_cast<std::uint32_t>(directorySize));
            Append32(zip, static_cast<std::uint32_t>(directoryOffset));
            Append16(zip, 0);
        }
        return zip;
    }

    const ZipPayload *FindPayload(const std::vector<ZipPayload> &payloads, const std::string &name) {
        for (const ZipPayload &payload: payloads) {
            if (payload.Name == name) {
                return &payload;
            }
        }
        return nullptr;
    }
}

TEST_CASE("IsSafeArchiveEntry rejects traversal and absolute paths", "[archive][safety]") {
    using namespace CoreDeck::Platform::detail;

    REQUIRE(IsSafeArchiveEntry("cmdline-tools/bin/sdkmanager"));
    REQUIRE(IsSafeArchiveEntry("./cmdline-tools/NOTICE.txt"));

    REQUIRE_FALSE(IsSafeArchiveEntry(""));
    REQUIRE_FALSE(IsSafeArchiveEntry("../escape.txt"));
    REQUIRE_FALSE(IsSafeArchiveEntry("cmdline-tools/../../escape.txt"));
    REQUIRE_FALSE(IsSafeArchiveEntry("/etc/passwd"));
    REQUIRE_FALSE(IsSafeArchiveEntry("\\windows\\system32"));
    REQUIRE_FALSE(IsSafeArchiveEntry("C:/Windows/System32"));
    REQUIRE_FALSE(IsSafeArchiveEntry("..\\escape.txt"));
}

TEST_CASE("StripLeadingComponent drops the first path segment", "[archive][safety]") {
    using namespace CoreDeck::Platform::detail;

    REQUIRE(StripLeadingComponent("cmdline-tools/bin/sdkmanager") == "bin/sdkmanager");
    REQUIRE(StripLeadingComponent("cmdline-tools/").empty());
    REQUIRE(StripLeadingComponent("cmdline-tools").empty());
}

TEST_CASE("ExtractZip writes entries relative to the destination", "[archive][extract]") {
    const std::filesystem::path scratch = MakeScratchDir("plain");
    const std::filesystem::path zipPath = scratch / "test.zip";
    const std::filesystem::path dest = scratch / "out";

    REQUIRE(WriteZip(zipPath, {{"root/a.txt", "alpha"}, {"root/sub/b.txt", "beta"}}));

    std::string error;
    REQUIRE(Platform::ExtractZip(zipPath.string(), dest.string(), Platform::ExtractOptions{}, nullptr, error));
    REQUIRE(error.empty());

    REQUIRE(std::filesystem::exists(dest / "root" / "a.txt"));
    REQUIRE(ReadFile(dest / "root" / "a.txt") == "alpha");
    REQUIRE(ReadFile(dest / "root" / "sub" / "b.txt") == "beta");

    std::filesystem::remove_all(scratch);
}

TEST_CASE("ExtractZip can strip the archive's top-level directory", "[archive][extract]") {
    const std::filesystem::path scratch = MakeScratchDir("strip");
    const std::filesystem::path zipPath = scratch / "test.zip";
    const std::filesystem::path dest = scratch / "latest";

    REQUIRE(WriteZip(zipPath, {{"cmdline-tools/bin/sdkmanager", "#!/bin/sh"}, {"cmdline-tools/lib/x.jar", "jar"}}));

    std::string error;
    REQUIRE(Platform::ExtractZip(zipPath.string(), dest.string(), Platform::ExtractOptions{.StripTopLevelDir = true}, nullptr, error));

    REQUIRE(std::filesystem::exists(dest / "bin" / "sdkmanager"));
    REQUIRE(std::filesystem::exists(dest / "lib" / "x.jar"));
    REQUIRE_FALSE(std::filesystem::exists(dest / "cmdline-tools"));

    std::filesystem::remove_all(scratch);
}

TEST_CASE("ExtractZip refuses entries that escape the destination", "[archive][safety]") {
    const std::filesystem::path scratch = MakeScratchDir("slip");
    const std::filesystem::path zipPath = scratch / "evil.zip";
    const std::filesystem::path dest = scratch / "out";

    REQUIRE(WriteZip(zipPath, {{"safe.txt", "fine"}, {"../escape.txt", "pwned"}}));

    std::string error;
    REQUIRE_FALSE(Platform::ExtractZip(zipPath.string(), dest.string(), Platform::ExtractOptions{}, nullptr, error));
    REQUIRE_FALSE(error.empty());
    REQUIRE_FALSE(std::filesystem::exists(scratch / "escape.txt"));

    std::filesystem::remove_all(scratch);
}

TEST_CASE("ExtractZip reports progress and honours cancellation", "[archive][extract]") {
    const std::filesystem::path scratch = MakeScratchDir("cancel");
    const std::filesystem::path zipPath = scratch / "test.zip";
    const std::filesystem::path dest = scratch / "out";

    REQUIRE(WriteZip(zipPath, {{"a.txt", "1"}, {"b.txt", "2"}, {"c.txt", "3"}}));

    int calls = 0;
    std::string error;
    const bool ok = Platform::ExtractZip(
        zipPath.string(),
        dest.string(),
        Platform::ExtractOptions(),
        [&calls](const float progress) {
            ++calls;
            REQUIRE(progress > 0.0F);
            REQUIRE(progress <= 1.0F);
            return calls < 2;
        },
        error
    );

    REQUIRE_FALSE(ok);
    REQUIRE(calls == 2);
    REQUIRE_FALSE(std::filesystem::exists(dest / "c.txt"));

    std::filesystem::remove_all(scratch);
}

#ifndef _WIN32
TEST_CASE("ExtractZip marks bin/ entries executable", "[archive][permissions]") {
    const std::filesystem::path scratch = MakeScratchDir("perms");
    const std::filesystem::path zipPath = scratch / "tools.zip";
    const std::filesystem::path dest = scratch / "latest";

    REQUIRE(WriteZip(zipPath, {{"cmdline-tools/bin/sdkmanager", "#!/bin/sh\n"}, {"cmdline-tools/lib/x.jar", "jar"}}));

    std::string error;
    REQUIRE(Platform::ExtractZip(zipPath.string(), dest.string(), Platform::ExtractOptions{.StripTopLevelDir = true}, nullptr, error));

    const auto perms = std::filesystem::status(dest / "bin" / "sdkmanager").permissions();
    REQUIRE((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);

    std::filesystem::remove_all(scratch);
}
#endif

TEST_CASE("ReadZipPayloads reports the stored payload offset", "[archive][zip]") {
    const std::filesystem::path scratch = MakeScratchDir("payload");
    const std::filesystem::path zipPath = scratch / "stored.zip";
    const std::vector<unsigned char> bytes = StoredZip({{"a.txt", "hello"}, {"dir/", ""}}, false);
    {
        std::ofstream output(zipPath, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<ZipPayload> payloads;
    std::string error;
    REQUIRE(ReadZipPayloads(zipPath.string(), payloads, error));
    REQUIRE(error.empty());
    const ZipPayload *file = FindPayload(payloads, "a.txt");
    const ZipPayload *directory = FindPayload(payloads, "dir/");
    REQUIRE(file != nullptr);
    REQUIRE(directory != nullptr);
    REQUIRE(file->Stored);
    REQUIRE_FALSE(file->Directory);
    REQUIRE(directory->Directory);
    REQUIRE(file->DataOffset == 35);

    std::filesystem::remove_all(scratch);
}

TEST_CASE("ReadZipPayloads reads a Zip64 local header offset", "[archive][zip]") {
    const std::filesystem::path scratch = MakeScratchDir("zip64");
    const std::filesystem::path zipPath = scratch / "zip64.zip";
    const std::vector<unsigned char> bytes = StoredZip({{"a.txt", "hello"}}, true);
    {
        std::ofstream output(zipPath, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<ZipPayload> payloads;
    std::string error;
    REQUIRE(ReadZipPayloads(zipPath.string(), payloads, error));
    REQUIRE(payloads.size() == 1);
    REQUIRE(payloads.front().Name == "a.txt");
    REQUIRE(payloads.front().DataOffset == 35);
    REQUIRE(payloads.front().Stored);

    std::filesystem::remove_all(scratch);
}

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <miniz.h>

#include "core/archive.h"

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
        mz_zip_archive zip = {};
        if (mz_zip_writer_init_file(&zip, zipPath.string().c_str(), 0) == 0) {
            return false;
        }
        bool ok = true;
        for (const auto &[name, content]: entries) {
            if (!mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(), MZ_BEST_SPEED)) {
                ok = false;
                break;
            }
        }
        ok = (mz_zip_writer_finalize_archive(&zip) != 0) && ok;
        mz_zip_writer_end(&zip);
        return ok;
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator(in), std::istreambuf_iterator<char>()};
    }
}

TEST_CASE("IsSafeArchiveEntry rejects traversal and absolute paths", "[archive][safety]") {
    using namespace CoreDeck::detail;

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
    using namespace CoreDeck::detail;

    REQUIRE(StripLeadingComponent("cmdline-tools/bin/sdkmanager") == "bin/sdkmanager");
    REQUIRE(StripLeadingComponent("cmdline-tools/") == "");
    REQUIRE(StripLeadingComponent("cmdline-tools").empty());
}

TEST_CASE("ExtractZip writes entries relative to the destination", "[archive][extract]") {
    const std::filesystem::path scratch = MakeScratchDir("plain");
    const std::filesystem::path zipPath = scratch / "test.zip";
    const std::filesystem::path dest = scratch / "out";

    REQUIRE(WriteZip(zipPath, {{"root/a.txt", "alpha"}, {"root/sub/b.txt", "beta"}}));

    std::string error;
    REQUIRE(ExtractZip(zipPath.string(), dest.string(), ExtractOptions{}, nullptr, error));
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
    REQUIRE(ExtractZip(
        zipPath.string(),
        dest.string(),
        ExtractOptions{.StripTopLevelDir = true},
        nullptr,
        error
    ));

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
    REQUIRE_FALSE(ExtractZip(zipPath.string(), dest.string(), ExtractOptions{}, nullptr, error));
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
    const bool ok = ExtractZip(
        zipPath.string(),
        dest.string(),
        ExtractOptions{},
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

#if !defined(_WIN32)
TEST_CASE("ExtractZip marks bin/ entries executable", "[archive][permissions]") {
    const std::filesystem::path scratch = MakeScratchDir("perms");
    const std::filesystem::path zipPath = scratch / "tools.zip";
    const std::filesystem::path dest = scratch / "latest";

    REQUIRE(WriteZip(zipPath, {{"cmdline-tools/bin/sdkmanager", "#!/bin/sh\n"}, {"cmdline-tools/lib/x.jar", "jar"}}));

    std::string error;
    REQUIRE(ExtractZip(
        zipPath.string(),
        dest.string(),
        ExtractOptions{.StripTopLevelDir = true},
        nullptr,
        error
    ));

    const auto perms = std::filesystem::status(dest / "bin" / "sdkmanager").permissions();
    REQUIRE((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);

    std::filesystem::remove_all(scratch);
}
#endif

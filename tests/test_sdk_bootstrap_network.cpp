#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

#include "core/archive.h"
#include "core/http_download.h"
#include "core/sdk_bootstrap.h"
#include "core/sha256.h"

using namespace CoreDeck;

namespace {
    bool NetworkTestsEnabled() {
        const char *flag = std::getenv("COREDECK_TEST_NETWORK"); // NOLINT(concurrency-mt-unsafe)
        return flag != nullptr && std::string(flag) == "1";
    }
}

TEST_CASE("The pinned cmdline-tools archive downloads and matches its checksum", "[.network][bootstrap]") {
    if (!NetworkTestsEnabled()) {
        SKIP("Set COREDECK_TEST_NETWORK=1 to run network tests.");
    }

    const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();
    REQUIRE_FALSE(release.DownloadUrl.empty());

    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "coredeck_bootstrap_network";
    std::filesystem::remove_all(scratch);
    std::filesystem::create_directories(scratch);

    const std::filesystem::path archive = scratch / release.FileName;

    std::string error;
    const bool downloaded = HttpDownloadToFile(
        release.DownloadUrl,
        archive.string(),
        "CoreDeck-tests",
        nullptr,
        error
    );
    INFO("download error: " << error);
    REQUIRE(downloaded);

    REQUIRE(std::filesystem::file_size(archive) == release.DownloadSize);
    REQUIRE(EqualsIgnoreCaseHex(Sha256File(archive.string()), release.Sha256));

    const std::filesystem::path extracted = scratch / "latest";
    REQUIRE(ExtractZip(
        archive.string(),
        extracted.string(),
        ExtractOptions{.StripTopLevelDir = true},
        nullptr,
        error
    ));

#if defined(_WIN32)
    REQUIRE(std::filesystem::exists(extracted / "bin" / "sdkmanager.bat"));
#else
    REQUIRE(std::filesystem::exists(extracted / "bin" / "sdkmanager"));
    const auto perms = std::filesystem::status(extracted / "bin" / "sdkmanager").permissions();
    REQUIRE((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
#endif

    std::filesystem::remove_all(scratch);
}

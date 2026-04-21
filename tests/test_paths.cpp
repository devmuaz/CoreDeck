#include <catch2/catch_test_macros.hpp>

#include "core/paths.h"

using namespace CoreDeck::Paths;

TEST_CASE("JoinPaths joins components with the platform separator", "[paths][join]") {
    const std::string joined = JoinPaths({"a", "b", "c"});
#ifdef _WIN32
    REQUIRE(joined == "a\\b\\c");
#else
    REQUIRE(joined == "a/b/c");
#endif
}

TEST_CASE("JoinPaths skips empty components", "[paths][join]") {
    const std::string joined = JoinPaths({"root", "", "leaf"});
#ifdef _WIN32
    REQUIRE(joined == "root\\leaf");
#else
    REQUIRE(joined == "root/leaf");
#endif
}

TEST_CASE("JoinPaths returns empty for empty input", "[paths][join]") {
    REQUIRE(JoinPaths({}).empty());
}

TEST_CASE("JoinPaths with a single component returns it unchanged", "[paths][join]") {
    REQUIRE(JoinPaths({"solo"}) == "solo");
}

TEST_CASE("NormalizePath collapses redundant segments", "[paths][normalize]") {
#ifdef _WIN32
    REQUIRE(NormalizePath("a\\b\\..\\c") == "a\\c");
    REQUIRE(NormalizePath("a\\.\\b") == "a\\b");
#else
    REQUIRE(NormalizePath("a/b/../c") == "a/c");
    REQUIRE(NormalizePath("a/./b") == "a/b");
#endif
}

TEST_CASE("NormalizePath leaves an already-clean path alone", "[paths][normalize]") {
#ifdef _WIN32
    REQUIRE(NormalizePath("a\\b\\c") == "a\\b\\c");
#else
    REQUIRE(NormalizePath("a/b/c") == "a/b/c");
#endif
}

TEST_CASE("GetCurrentPlatform reports the platform this test runs on", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(GetCurrentPlatform() == Platform::Windows);
#elif defined(__APPLE__)
    REQUIRE(GetCurrentPlatform() == Platform::macOS);
#elif defined(__linux__)
    REQUIRE(GetCurrentPlatform() == Platform::Linux);
#endif
}

TEST_CASE("GetPlatformName returns a non-empty, known string", "[paths][platform]") {
    const std::string name = GetPlatformName();
    REQUIRE_FALSE(name.empty());
    REQUIRE((name == "Windows" || name == "macOS" || name == "Linux" || name == "Unknown"));
}

TEST_CASE("GetExecutableExtension matches platform convention", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(GetExecutableExtension() == ".exe");
#else
    REQUIRE(GetExecutableExtension().empty());
#endif
}

TEST_CASE("GetNullDevice returns the correct per-platform device", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(GetNullDevice() == "NUL");
#else
    REQUIRE(GetNullDevice() == "/dev/null");
#endif
}
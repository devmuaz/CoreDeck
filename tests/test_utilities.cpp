#include <catch2/catch_test_macros.hpp>

#include "platform/utilities.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck;

TEST_CASE("FormatFileSize renders bytes under 1 KB as raw B", "[utilities][format]") {
    REQUIRE(Platform::FormatFileSize(0) == "0 B");
    REQUIRE(Platform::FormatFileSize(1) == "1 B");
    REQUIRE(Platform::FormatFileSize(1023) == "1023 B");
}

TEST_CASE("FormatFileSize renders KB/MB/GB with the right precision", "[utilities][format]") {
    REQUIRE(Platform::FormatFileSize(1024) == "1.0 KB");
    REQUIRE(Platform::FormatFileSize(1536) == "1.5 KB");
    REQUIRE(Platform::FormatFileSize(1024ULL * 1024) == "1.0 MB");
    REQUIRE(Platform::FormatFileSize(1024ULL * 1024 * 5) == "5.0 MB");
    REQUIRE(Platform::FormatFileSize(1024ULL * 1024 * 1024) == "1.00 GB");
    REQUIRE(Platform::FormatFileSize((1024ULL * 1024 * 1024 * 3) + (1024ULL * 1024 * 512)) == "3.50 GB");
}

TEST_CASE("FormatFileSize picks the largest fitting unit at boundaries", "[utilities][format]") {
    REQUIRE(Platform::FormatFileSize((1024 * 1024) - 1) == "1024.0 KB");
    REQUIRE(Platform::FormatFileSize((1024ULL * 1024 * 1024) - 1) == "1024.0 MB");
}

TEST_CASE("StrConcat joins heterogeneous string-like inputs", "[utilities][strconcat]") {
    REQUIRE(Platform::StrConcat("hello", " ", "world") == "hello world");
    REQUIRE(Platform::StrConcat(std::string("foo"), "-", std::string("bar")) == "foo-bar");
}

TEST_CASE("StrConcat with a single argument passes it through", "[utilities][strconcat]") {
    REQUIRE(Platform::StrConcat("only") == "only");
}

#include <catch2/catch_test_macros.hpp>

#include "core/utilities.h"

using namespace CoreDeck;

TEST_CASE("FormatFileSize renders bytes under 1 KB as raw B", "[utilities][format]") {
    REQUIRE(FormatFileSize(0) == "0 B");
    REQUIRE(FormatFileSize(1) == "1 B");
    REQUIRE(FormatFileSize(1023) == "1023 B");
}

TEST_CASE("FormatFileSize renders KB/MB/GB with the right precision", "[utilities][format]") {
    REQUIRE(FormatFileSize(1024) == "1.0 KB");
    REQUIRE(FormatFileSize(1536) == "1.5 KB");
    REQUIRE(FormatFileSize(1024ULL * 1024) == "1.0 MB");
    REQUIRE(FormatFileSize(1024ULL * 1024 * 5) == "5.0 MB");
    REQUIRE(FormatFileSize(1024ULL * 1024 * 1024) == "1.00 GB");
    REQUIRE(FormatFileSize(1024ULL * 1024 * 1024 * 3 + 1024ULL * 1024 * 512) == "3.50 GB");
}

TEST_CASE("FormatFileSize picks the largest fitting unit at boundaries", "[utilities][format]") {
    REQUIRE(FormatFileSize(1024 * 1024 - 1) == "1024.0 KB");
    REQUIRE(FormatFileSize(1024ULL * 1024 * 1024 - 1) == "1024.0 MB");
}

TEST_CASE("StrConcat joins heterogeneous string-like inputs", "[utilities][strconcat]") {
    REQUIRE(StrConcat("hello", " ", "world") == "hello world");
    REQUIRE(StrConcat(std::string("foo"), "-", std::string("bar")) == "foo-bar");
}

TEST_CASE("StrConcat with a single argument passes it through", "[utilities][strconcat]") {
    REQUIRE(StrConcat("only") == "only");
}

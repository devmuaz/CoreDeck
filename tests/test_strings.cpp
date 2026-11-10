#include <catch2/catch_test_macros.hpp>

#include "platform/strings.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck;

TEST_CASE("StrConcat joins heterogeneous string-like inputs", "[strings][strconcat]") {
    REQUIRE(Platform::StrConcat("hello", " ", "world") == "hello world");
    REQUIRE(Platform::StrConcat(std::string("foo"), "-", std::string("bar")) == "foo-bar");
}

TEST_CASE("StrConcat with a single argument passes it through", "[strings][strconcat]") {
    REQUIRE(Platform::StrConcat("only") == "only");
}

TEST_CASE("LowerCopy and UpperCopy change ASCII letters only", "[strings][case]") {
    REQUIRE(Platform::LowerCopy("Key.JKS-1") == "key.jks-1");
    REQUIRE(Platform::UpperCopy("key.jks-1") == "KEY.JKS-1");
}

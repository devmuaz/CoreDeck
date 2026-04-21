#include <catch2/catch_test_macros.hpp>

#include "core/version_check.h"

using namespace CoreDeck::detail;

TEST_CASE("CompareSemanticVersion reports equality", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("1.0.0", "1.0.0") == 0);
    REQUIRE(CompareSemanticVersion("0.0.0", "0.0.0") == 0);
}

TEST_CASE("CompareSemanticVersion orders by major, minor, patch", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("2.0.0", "1.9.9") == 1);
    REQUIRE(CompareSemanticVersion("1.2.0", "1.1.9") == 1);
    REQUIRE(CompareSemanticVersion("1.0.2", "1.0.1") == 1);

    REQUIRE(CompareSemanticVersion("1.0.0", "2.0.0") == -1);
    REQUIRE(CompareSemanticVersion("1.1.0", "1.2.0") == -1);
    REQUIRE(CompareSemanticVersion("1.0.1", "1.0.2") == -1);
}

TEST_CASE("CompareSemanticVersion compares numerically, not lexically", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("2.10.0", "2.2.0") == 1);
    REQUIRE(CompareSemanticVersion("1.0.10", "1.0.9") == 1);
    REQUIRE(CompareSemanticVersion("11.0.0", "9.9.9") == 1);
}

TEST_CASE("CompareSemanticVersion strips leading v/V", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("v1.2.3", "1.2.3") == 0);
    REQUIRE(CompareSemanticVersion("V1.2.3", "1.2.3") == 0);
    REQUIRE(CompareSemanticVersion("v2.0.0", "v1.0.0") == 1);
}

TEST_CASE("CompareSemanticVersion pads missing components with zero", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("1.0", "1.0.0") == 0);
    REQUIRE(CompareSemanticVersion("1", "1.0.0") == 0);
    REQUIRE(CompareSemanticVersion("2.0", "1.9.9") == 1);
}

TEST_CASE("CompareSemanticVersion treats pre-release as lower than matching stable", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("1.0.0-beta", "1.0.0") == -1);
    REQUIRE(CompareSemanticVersion("1.0.0", "1.0.0-beta") == 1);
    REQUIRE(CompareSemanticVersion("1.0.0-rc1", "1.0.0-alpha") == 0);
    REQUIRE(CompareSemanticVersion("2.0.0-beta", "1.9.9") == 1);
    REQUIRE(CompareSemanticVersion("1.0.0-beta", "2.0.0") == -1);
}

TEST_CASE("CompareSemanticVersion handles empty strings as 0.0.0", "[version_check][semver]") {
    REQUIRE(CompareSemanticVersion("", "1.0.0") == -1);
    REQUIRE(CompareSemanticVersion("1.0.0", "") == 1);
    REQUIRE(CompareSemanticVersion("", "") == 0);
}

TEST_CASE("ParseLatestReleaseTag extracts tag_name from a valid GitHub response", "[version_check][parse]") {
    const auto tag = ParseLatestReleaseTag(R"({"tag_name":"v1.2.3"})");
    REQUIRE(tag.has_value());
    REQUIRE(tag.value() == "v1.2.3");
}

TEST_CASE("ParseLatestReleaseTag still parses when extra fields are present", "[version_check][parse]") {
    const auto tag = ParseLatestReleaseTag(
        R"({"tag_name":"v2.0.0","name":"Release 2.0","body":"notes","draft":false})"
    );
    REQUIRE(tag.has_value());
    REQUIRE(tag.value() == "v2.0.0");
}

TEST_CASE("ParseLatestReleaseTag rejects an empty tag_name", "[version_check][parse]") {
    REQUIRE_FALSE(ParseLatestReleaseTag(R"({"tag_name":""})").has_value());
}

TEST_CASE("ParseLatestReleaseTag rejects malformed JSON", "[version_check][parse]") {
    REQUIRE_FALSE(ParseLatestReleaseTag("not json at all").has_value());
    REQUIRE_FALSE(ParseLatestReleaseTag("{").has_value());
    REQUIRE_FALSE(ParseLatestReleaseTag("").has_value());
}

TEST_CASE("ParseLatestReleaseTag rejects JSON missing tag_name", "[version_check][parse]") {
    REQUIRE_FALSE(ParseLatestReleaseTag(R"({"name":"Release 1.0"})").has_value());
}
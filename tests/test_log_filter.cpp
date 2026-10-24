#include <catch2/catch_test_macros.hpp>

#include "platform/log_filter.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck;

namespace {
    std::vector<std::string> Sample() {
        return {
            "INFO         | start emulator",
            "WARNING      | hvf is not enabled",
            "ERROR        | mprotect failed: Permission denied",
            "INFO         | retrying...",
        };
    }

    std::string Slice(const std::string &joined, const Platform::LogMatch &m) {
        return joined.substr(m.StartOffset, m.EndOffset - m.StartOffset);
    }
}

TEST_CASE("FilterLog with empty query returns joined text and no matches", "[log_filter][substring]") {
    const auto result = Platform::FilterLog(Sample(), {});
    REQUIRE(result.Matches.empty());
    REQUIRE(result.RegexValid);
    REQUIRE(result.Joined.find("hvf is not enabled") != std::string::npos);
    REQUIRE(result.Joined.back() == '\n');
}

TEST_CASE("FilterLog substring match is case-insensitive by default", "[log_filter][substring]") {
    const auto result = Platform::FilterLog(Sample(), {.Query = "WARNING"});
    REQUIRE(result.Matches.size() == 1);
    REQUIRE(Slice(result.Joined, result.Matches.at(0)) == "WARNING");

    const auto lowered = Platform::FilterLog(Sample(), {.Query = "warning"});
    REQUIRE(lowered.Matches.size() == 1);
    REQUIRE(Slice(lowered.Joined, lowered.Matches.at(0)) == "WARNING");
}

TEST_CASE("FilterLog substring match honors case-sensitive flag", "[log_filter][substring]") {
    Platform::LogFilterOptions opts;
    opts.Query = "warning";
    opts.CaseSensitive = true;
    const auto result = Platform::FilterLog(Sample(), opts);
    REQUIRE(result.Matches.empty());
}

TEST_CASE("FilterLog finds multiple substring occurrences across lines", "[log_filter][substring]") {
    const auto result = Platform::FilterLog(Sample(), {.Query = "INFO"});
    REQUIRE(result.Matches.size() == 2);
    for (const auto &m: result.Matches) {
        REQUIRE(Slice(result.Joined, m) == "INFO");
    }
    REQUIRE(result.Matches.at(0).StartOffset < result.Matches.at(1).StartOffset);
}

TEST_CASE("FilterLog regex compiles and matches", "[log_filter][regex]") {
    Platform::LogFilterOptions opts;
    opts.Query = "(WARNING|ERROR)";
    opts.UseRegex = true;
    const auto result = Platform::FilterLog(Sample(), opts);
    REQUIRE(result.RegexValid);
    REQUIRE(result.Matches.size() == 2);
    REQUIRE(Slice(result.Joined, result.Matches.at(0)) == "WARNING");
    REQUIRE(Slice(result.Joined, result.Matches.at(1)) == "ERROR");
}

TEST_CASE("FilterLog reports invalid regex without crashing", "[log_filter][regex]") {
    Platform::LogFilterOptions opts;
    opts.Query = "(unclosed";
    opts.UseRegex = true;
    const auto [Joined, Matches, RegexValid, RegexError] = Platform::FilterLog(Sample(), opts);
    REQUIRE_FALSE(RegexValid);
    REQUIRE_FALSE(RegexError.empty());
    REQUIRE(Matches.empty());
    REQUIRE(Joined.find("hvf is not enabled") != std::string::npos);
}

TEST_CASE("FilterLog regex zero-width matches are skipped", "[log_filter][regex]") {
    Platform::LogFilterOptions opts;
    opts.Query = "^";
    opts.UseRegex = true;
    const auto result = Platform::FilterLog({"a", "b"}, opts);
    REQUIRE(result.RegexValid);
    REQUIRE(result.Matches.empty());
}

TEST_CASE("FilterLog handles empty input", "[log_filter][edge]") {
    const auto result = Platform::FilterLog({}, {.Query = "anything"});
    REQUIRE(result.Joined.empty());
    REQUIRE(result.Matches.empty());
}
#include <catch2/catch_test_macros.hpp>

#include "core/log_buffer.h"

using namespace CoreDeck;

TEST_CASE("LogBuffer starts empty with no new content", "[log_buffer]") {
    LogBuffer buf;
    REQUIRE(buf.GetLines().empty());
    REQUIRE_FALSE(buf.HasNewContent());
}

TEST_CASE("LogBuffer::Push appends lines in order", "[log_buffer][push]") {
    LogBuffer buf;
    buf.Push("first");
    buf.Push("second");
    buf.Push("third");

    const auto lines = buf.GetLines();
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0] == "first");
    REQUIRE(lines[1] == "second");
    REQUIRE(lines[2] == "third");
}

TEST_CASE("LogBuffer::Push flips HasNewContent to true", "[log_buffer][new-content]") {
    LogBuffer buf;
    REQUIRE_FALSE(buf.HasNewContent());
    buf.Push("line");
    REQUIRE(buf.HasNewContent());
}

TEST_CASE("LogBuffer::ResetNewContentFlag clears HasNewContent without touching lines", "[log_buffer][new-content]") {
    LogBuffer buf;
    buf.Push("line");
    REQUIRE(buf.HasNewContent());

    buf.ResetNewContentFlag();
    REQUIRE_FALSE(buf.HasNewContent());
    REQUIRE(buf.GetLines().size() == 1);
}

TEST_CASE("LogBuffer::Clear empties the buffer and resets HasNewContent", "[log_buffer][clear]") {
    LogBuffer buf;
    buf.Push("line");
    REQUIRE_FALSE(buf.GetLines().empty());
    REQUIRE(buf.HasNewContent());

    buf.Clear();
    REQUIRE(buf.GetLines().empty());
    REQUIRE_FALSE(buf.HasNewContent());
}

TEST_CASE("LogBuffer evicts the oldest line when maxLines is exceeded (FIFO)", "[log_buffer][eviction]") {
    LogBuffer buf(3);
    buf.Push("a");
    buf.Push("b");
    buf.Push("c");
    buf.Push("d");

    const auto lines = buf.GetLines();
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0] == "b");
    REQUIRE(lines[1] == "c");
    REQUIRE(lines[2] == "d");
}

TEST_CASE("LogBuffer eviction holds steady at maxLines across many pushes", "[log_buffer][eviction]") {
    LogBuffer buf(5);
    for (int i = 0; i < 20; ++i) {
        buf.Push("line_" + std::to_string(i));
    }

    const auto lines = buf.GetLines();
    REQUIRE(lines.size() == 5);
    REQUIRE(lines.front() == "line_15");
    REQUIRE(lines.back() == "line_19");
}

TEST_CASE("LogBuffer with maxLines=1 keeps only the newest line", "[log_buffer][eviction]") {
    LogBuffer buf(1);
    buf.Push("old");
    buf.Push("new");

    const auto lines = buf.GetLines();
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0] == "new");
}
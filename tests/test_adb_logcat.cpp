#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "core/adb_logcat.h"
#include "platform/process.h"

using namespace CoreDeck;
using namespace CoreDeck::Core;

TEST_CASE("BuildAdbLogcatArguments keeps device and filters explicit", "[adb][logcat]") {
    AdbLogcatOptions options;
    options.Buffers = {"main", "crash"};
    options.MinimumPriority = AdbLogPriority::Warning;
    options.TailLines = 50;

    REQUIRE(
        BuildAdbLogcatArguments("emulator-5554", options) ==
        std::vector<std::string>{
            "-s", "emulator-5554", "logcat", "-v", "threadtime",
            "-b", "main", "-b", "crash", "-T", "50", "*:W"
        }
    );
}

TEST_CASE("LineStreamDecoder preserves partial lines and CRLF", "[process]") {
    Platform::LineStreamDecoder decoder;
    std::vector<std::string> lines;
    const auto collect = [&lines](const std::string &line) { lines.push_back(line); };

    decoder.Push("first\r", 6, collect);
    decoder.Push("\nsec", 4, collect);
    decoder.Push("ond\n\nlast", 9, collect);
    decoder.Flush(collect);

    REQUIRE(lines == std::vector<std::string>{"first", "second", "", "last"});
}

TEST_CASE("LogBuffer reports bounded drops and revisions", "[log]") {
    Platform::LogBuffer log(2);
    const auto initial = log.Revision();
    log.Push("one");
    log.Push("two");
    log.Push("three");

    REQUIRE(log.GetLines() == std::vector<std::string>{"two", "three"});
    REQUIRE(log.DroppedLines() == 1);
    REQUIRE(log.Revision() == initial + 3);
    log.Clear();
    REQUIRE(log.DroppedLines() == 0);
}

TEST_CASE("AdbLogcatSession separates diagnostics and retains captured lines", "[adb][logcat]") {
    SdkInfo sdk;
    sdk.AdbPath = COREDECK_PROCESS_FIXTURE;
    AdbLogcatSession session;

    REQUIRE(session.Start(sdk, "emulator-5554", {}));
    for (int attempt = 0; attempt < 100 && session.Status().Running; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    session.Stop();

    const auto lines = session.Log()->GetLines();
    REQUIRE(lines.size() == 2);
    REQUIRE(lines.front().find("first") != std::string::npos);
    REQUIRE(session.Status().Error == "logcat diagnostic");
}

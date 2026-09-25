#include <catch2/catch_test_macros.hpp>

#include "core/apk_analyzer.h"

using namespace CoreDeck;

TEST_CASE("ParseApkSummary reads the tab-separated summary row", "[apk-analyzer]") {
    const auto summary = ParseApkSummary("com.example.helloworld\t1\t1.0\n");

    REQUIRE(summary.has_value());
    REQUIRE(summary->ApplicationId == "com.example.helloworld");
    REQUIRE(summary->VersionCode == "1");
    REQUIRE(summary->VersionName == "1.0");
}

TEST_CASE("ParseApkSummary keeps a version name that contains spaces", "[apk-analyzer]") {
    const auto summary = ParseApkSummary("Warning: package.xml parsing problem\ncom.example.app\t42\t1.2 beta\r\n");

    REQUIRE(summary.has_value());
    REQUIRE(summary->VersionCode == "42");
    REQUIRE(summary->VersionName == "1.2 beta");
}

TEST_CASE("ParseApkSummary rejects a row that is not three fields", "[apk-analyzer]") {
    REQUIRE_FALSE(ParseApkSummary("com.example.helloworld\n").has_value());
    REQUIRE_FALSE(ParseApkSummary("").has_value());
}

TEST_CASE("ParseApkByteSize reads a byte count", "[apk-analyzer]") {
    REQUIRE(ParseApkByteSize("5047\n") == 5047);
    REQUIRE(ParseApkByteSize("3952\n") == 3952);
    REQUIRE_FALSE(ParseApkByteSize("12.3MB\n").has_value());
}

TEST_CASE("ParseApkDebuggable reads true and false", "[apk-analyzer]") {
    REQUIRE(ParseApkDebuggable("true\n") == true);
    REQUIRE(ParseApkDebuggable("false\n") == false);
    REQUIRE_FALSE(ParseApkDebuggable("yes\n").has_value());
}

TEST_CASE("ParseApkFileList reads raw size, download size, and path", "[apk-analyzer]") {
    const auto files = ParseApkFileList(
        "4157\t3476\t/\n"
        "703\t703\t/classes.dex\n"
        "312\t312\t/res/layout/main.xml\r\n"
    );

    REQUIRE(files.has_value());
    REQUIRE(files->size() == 3);
    REQUIRE(files->at(0).Path == "/");
    REQUIRE(files->at(0).RawSize == 4157);
    REQUIRE(files->at(0).DownloadSize == 3476);
    REQUIRE(files->at(1).Path == "/classes.dex");
    REQUIRE(files->at(2).Path == "/res/layout/main.xml");
    REQUIRE(files->at(2).RawSize == 312);
}

TEST_CASE("ParseApkFileList rejects a row without both size columns", "[apk-analyzer]") {
    REQUIRE_FALSE(ParseApkFileList("703\t/classes.dex\n").has_value());
}

TEST_CASE("ParseApkCompare reads old, new, and signed difference", "[apk-analyzer]") {
    const auto rows = ParseApkCompare(
        "960\t649\t-311\t/\n"
        "352\t0\t-352\t/instant-run.zip\n"
        "6\t6\t0\t/res/anim/fade.xml\n"
    );

    REQUIRE(rows.has_value());
    REQUIRE(rows->size() == 3);
    REQUIRE(rows->at(0).Path == "/");
    REQUIRE(rows->at(0).OldSize == 960);
    REQUIRE(rows->at(0).NewSize == 649);
    REQUIRE(rows->at(0).Difference == -311);
    REQUIRE(rows->at(1).Path == "/instant-run.zip");
    REQUIRE(rows->at(2).Difference == 0);
}

TEST_CASE("RunApkAnalyzer refuses to run without an apkanalyzer binary", "[apk-analyzer]") {
    const SdkInfo sdk;
    REQUIRE_FALSE(RunApkAnalyzer(sdk, {"apk", "summary", "app.apk"}).has_value());

    const auto summary = QueryApkSummary(sdk, "app.apk");
    REQUIRE(summary.ToolMissing);
    REQUIRE_FALSE(summary.Ok);
}

TEST_CASE("QueryApkSummary does not launch when the apk path is empty", "[apk-analyzer]") {
    SdkInfo sdk;
    sdk.ApkAnalyzerPath = "/sdk/cmdline-tools/latest/bin/apkanalyzer";

    const auto summary = QueryApkSummary(sdk, "");
    REQUIRE_FALSE(summary.ToolMissing);
    REQUIRE_FALSE(summary.Ok);
}

TEST_CASE("LoadApkReport reports a missing apkanalyzer without running it", "[apk-analyzer]") {
    const auto report = LoadApkReport(SdkInfo{}, "app.apk");

    REQUIRE(report.ToolMissing);
    REQUIRE(report.ApkPath == "app.apk");
    REQUIRE_FALSE(report.Summary.Ok);
    REQUIRE_FALSE(report.Debuggable.Ok);
    REQUIRE_FALSE(report.Files.Ok);
}

TEST_CASE("ZipDataAlignment reports the page or byte boundary", "[apk-analyzer]") {
    REQUIRE(ZipDataAlignment(0) == 0);
    REQUIRE(ZipDataAlignment(1) == 1);
    REQUIRE(ZipDataAlignment(4) == 4);
    REQUIRE(ZipDataAlignment(4096) == 4096);
    REQUIRE(ZipDataAlignment(16384) == 16384);
    REQUIRE(ZipDataAlignment(16384 + 4) == 4);
    REQUIRE(ZipDataAlignment(32768) == 16384);
}

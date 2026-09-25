//
// Created by AbdulMuaz Aqeel on 23/09/2026.
//

#ifndef COREDECK_APK_ANALYZER_H
#define COREDECK_APK_ANALYZER_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sdk.h"

namespace CoreDeck {
    struct ApkSummary {
        std::string ApplicationId;
        std::string VersionCode;
        std::string VersionName;
    };

    struct ApkFileEntry {
        std::string Path;
        std::int64_t RawSize = 0;
        std::int64_t DownloadSize = 0;
        std::uint32_t Alignment = 0;
        bool Stored = false;
    };

    struct ApkCompareEntry {
        std::string Path;
        std::int64_t OldSize = 0;
        std::int64_t NewSize = 0;
        std::int64_t Difference = 0;
    };

    template<typename T>
    struct ApkAnalyzerQuery {
        bool ToolMissing = false;
        bool Ok = false;
        std::string Output;
        T Value{};
    };

    std::optional<std::string> RunApkAnalyzer(const SdkInfo &sdk, const std::vector<std::string> &args);

    std::optional<ApkSummary> ParseApkSummary(const std::string &output);

    std::optional<std::int64_t> ParseApkByteSize(const std::string &output);

    std::optional<bool> ParseApkDebuggable(const std::string &output);

    std::optional<std::vector<ApkFileEntry>> ParseApkFileList(const std::string &output);

    std::optional<std::vector<ApkCompareEntry>> ParseApkCompare(const std::string &output);

    ApkAnalyzerQuery<ApkSummary> QueryApkSummary(const SdkInfo &sdk, const std::string &apkPath);

    ApkAnalyzerQuery<std::int64_t> QueryApkFileSize(const SdkInfo &sdk, const std::string &apkPath);

    ApkAnalyzerQuery<std::int64_t> QueryApkDownloadSize(const SdkInfo &sdk, const std::string &apkPath);

    ApkAnalyzerQuery<std::vector<ApkCompareEntry>> QueryApkCompare(
        const SdkInfo &sdk,
        const std::string &baselineApkPath,
        const std::string &apkPath
    );

    ApkAnalyzerQuery<bool> QueryApkDebuggable(const SdkInfo &sdk, const std::string &apkPath);

    ApkAnalyzerQuery<std::vector<ApkFileEntry>> QueryApkFiles(const SdkInfo &sdk, const std::string &apkPath);

    struct ApkReport {
        std::string ApkPath;
        bool ToolMissing = false;
        ApkAnalyzerQuery<ApkSummary> Summary;
        ApkAnalyzerQuery<std::int64_t> FileSize;
        ApkAnalyzerQuery<std::int64_t> DownloadSize;
        ApkAnalyzerQuery<bool> Debuggable;
        ApkAnalyzerQuery<std::vector<ApkFileEntry>> Files;
        bool Cancelled = false;
    };

    using ApkLoadProgressFn = bool (*)(void *user, float progress, int stage);

    std::uint32_t ZipDataAlignment(std::uint64_t dataOffset);

    void ApplyApkZipAlignment(std::vector<ApkFileEntry> &entries, const std::string &apkPath);

    ApkReport LoadApkReport(
        const SdkInfo &sdk,
        const std::string &apkPath,
        ApkLoadProgressFn onProgress = nullptr,
        void *progressUser = nullptr
    );

    ApkAnalyzerQuery<std::vector<ApkCompareEntry>> LoadApkCompare(
        const SdkInfo &sdk,
        const std::string &baselineApkPath,
        const std::string &apkPath
    );
}

#endif // COREDECK_APK_ANALYZER_H

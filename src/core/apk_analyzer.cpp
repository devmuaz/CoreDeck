//
// Created by AbdulMuaz Aqeel on 23/09/2026.
//

#include <algorithm>
#include <filesystem>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <miniz.h>

#include "apk_analyzer.h"
#include "process.h"

namespace CoreDeck {
    namespace {
        std::string TrimLine(std::string line) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            return line;
        }

        bool IsWarningLine(const std::string &line) {
            return line.starts_with("Warning:") || line.starts_with("WARNING:");
        }

        bool IsFailureLine(const std::string &line) {
            return line.starts_with("Exception in thread") || line.starts_with("ERROR:") || line.starts_with("Error:");
        }

        bool ApkAnalyzerFailed(const std::string &output) {
            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line)) {
                line = TrimLine(std::move(line));
                if (IsFailureLine(line)) {
                    return true;
                }
            }
            return false;
        }

        std::optional<std::string> FirstContentLine(const std::string &output) {
            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line)) {
                line = TrimLine(std::move(line));
                if (line.empty() || IsWarningLine(line)) {
                    continue;
                }
                return line;
            }
            return std::nullopt;
        }

        std::optional<std::int64_t> ParseByteToken(const std::string &token) {
            if (token.empty()) {
                return std::nullopt;
            }
            errno = 0;
            char *end = nullptr;
            const int64_t value = std::strtoll(token.c_str(), &end, 10);
            if (end == token.c_str() || end == nullptr || *end != '\0' || errno == ERANGE) {
                return std::nullopt;
            }
            return static_cast<std::int64_t>(value);
        }

        std::optional<std::vector<std::string>> SplitTabs(const std::string &line, const std::size_t count) {
            std::vector<std::string> parts;
            parts.reserve(count);
            std::size_t start = 0;
            for (std::size_t i = 0; i + 1 < count; ++i) {
                const auto tab = line.find('\t', start);
                if (tab == std::string::npos) {
                    return std::nullopt;
                }
                parts.push_back(line.substr(start, tab - start));
                start = tab + 1;
            }
            parts.push_back(line.substr(start));
            return parts;
        }

        template<typename T, typename Parse>
        ApkAnalyzerQuery<T> RunQuery(const SdkInfo &sdk, const std::vector<std::string> &args, Parse parse) {
            ApkAnalyzerQuery<T> result;
            const auto output = RunApkAnalyzer(sdk, args);
            if (!output) {
                result.ToolMissing = true;
                return result;
            }
            result.Output = *output;
            if (ApkAnalyzerFailed(result.Output)) {
                return result;
            }
            if (auto parsed = parse(result.Output)) {
                result.Ok = true;
                result.Value = std::move(*parsed);
            }
            return result;
        }

        std::vector<std::string> ApkArgs(const std::string &verb, const std::string &apkPath) {
            return {"apk", verb, apkPath};
        }

        std::vector<std::string> ManifestArgs(const std::string &verb, const std::string &apkPath) {
            return {"manifest", verb, apkPath};
        }
    }

    std::optional<std::string> RunApkAnalyzer(const SdkInfo &sdk, const std::vector<std::string> &args) {
        if (sdk.ApkAnalyzerPath.empty()) {
            return std::nullopt;
        }
        return RunCommandArgs(sdk.ApkAnalyzerPath, args, "", sdk.ToolEnv);
    }

    std::optional<ApkSummary> ParseApkSummary(const std::string &output) {
        const auto line = FirstContentLine(output);
        if (!line) {
            return std::nullopt;
        }
        const auto parts = SplitTabs(*line, 3);
        if (!parts || (*parts)[0].empty() || (*parts)[1].empty()) {
            return std::nullopt;
        }
        ApkSummary summary;
        summary.ApplicationId = (*parts)[0];
        summary.VersionCode = (*parts)[1];
        summary.VersionName = (*parts)[2];
        return summary;
    }

    std::optional<std::int64_t> ParseApkByteSize(const std::string &output) {
        const auto line = FirstContentLine(output);
        if (!line) {
            return std::nullopt;
        }
        return ParseByteToken(*line);
    }

    std::optional<bool> ParseApkDebuggable(const std::string &output) {
        const auto line = FirstContentLine(output);
        if (!line) {
            return std::nullopt;
        }
        if (*line == "true") {
            return true;
        }
        if (*line == "false") {
            return false;
        }
        return std::nullopt;
    }

    std::optional<std::vector<ApkFileEntry>> ParseApkFileList(const std::string &output) {
        std::vector<ApkFileEntry> entries;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimLine(std::move(line));
            if (line.empty() || IsWarningLine(line)) {
                continue;
            }
            const auto parts = SplitTabs(line, 3);
            if (!parts || (*parts)[2].empty()) {
                return std::nullopt;
            }
            const auto raw = ParseByteToken((*parts)[0]);
            const auto download = ParseByteToken((*parts)[1]);
            if (!raw || !download) {
                return std::nullopt;
            }
            ApkFileEntry entry;
            entry.RawSize = *raw;
            entry.DownloadSize = *download;
            entry.Path = (*parts)[2];
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    std::optional<std::vector<ApkCompareEntry>> ParseApkCompare(const std::string &output) {
        std::vector<ApkCompareEntry> entries;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimLine(std::move(line));
            if (line.empty() || IsWarningLine(line)) {
                continue;
            }
            const auto parts = SplitTabs(line, 4);
            if (!parts || (*parts)[3].empty()) {
                return std::nullopt;
            }
            const auto oldSize = ParseByteToken((*parts)[0]);
            const auto newSize = ParseByteToken((*parts)[1]);
            const auto difference = ParseByteToken((*parts)[2]);
            if (!oldSize || !newSize || !difference) {
                return std::nullopt;
            }
            ApkCompareEntry entry;
            entry.OldSize = *oldSize;
            entry.NewSize = *newSize;
            entry.Difference = *difference;
            entry.Path = (*parts)[3];
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    ApkAnalyzerQuery<ApkSummary> QueryApkSummary(const SdkInfo &sdk, const std::string &apkPath) {
        if (apkPath.empty()) {
            return {};
        }
        return RunQuery<ApkSummary>(sdk, ApkArgs("summary", apkPath), ParseApkSummary);
    }

    ApkAnalyzerQuery<std::int64_t> QueryApkFileSize(const SdkInfo &sdk, const std::string &apkPath) {
        if (apkPath.empty()) {
            return {};
        }
        return RunQuery<std::int64_t>(sdk, ApkArgs("file-size", apkPath), ParseApkByteSize);
    }

    ApkAnalyzerQuery<std::int64_t> QueryApkDownloadSize(const SdkInfo &sdk, const std::string &apkPath) {
        if (apkPath.empty()) {
            return {};
        }
        return RunQuery<std::int64_t>(sdk, ApkArgs("download-size", apkPath), ParseApkByteSize);
    }

    ApkAnalyzerQuery<std::vector<ApkCompareEntry>> QueryApkCompare(
        const SdkInfo &sdk,
        const std::string &baselineApkPath,
        const std::string &apkPath
    ) {
        if (baselineApkPath.empty() || apkPath.empty()) {
            return {};
        }
        return RunQuery<std::vector<ApkCompareEntry>>(
            sdk,
            {"apk", "compare", "--different-only", baselineApkPath, apkPath},
            ParseApkCompare
        );
    }

    ApkAnalyzerQuery<bool> QueryApkDebuggable(const SdkInfo &sdk, const std::string &apkPath) {
        if (apkPath.empty()) {
            return {};
        }
        return RunQuery<bool>(sdk, ManifestArgs("debuggable", apkPath), ParseApkDebuggable);
    }

    ApkAnalyzerQuery<std::vector<ApkFileEntry>> QueryApkFiles(const SdkInfo &sdk, const std::string &apkPath) {
        if (apkPath.empty()) {
            return {};
        }
        return RunQuery<std::vector<ApkFileEntry>>(
            sdk,
            {"files", "list", "--raw-size", "--download-size", apkPath},
            ParseApkFileList
        );
    }

    std::uint32_t ZipDataAlignment(const std::uint64_t dataOffset) {
        if (dataOffset == 0) {
            return 0;
        }
        std::uint32_t alignment = 1;
        std::uint64_t value = dataOffset;
        while ((value % 2) == 0 && alignment < 16384U) {
            value /= 2;
            alignment *= 2;
        }
        return alignment;
    }

    namespace {
        std::string ZipLookupKey(std::string path) {
            for (char &character: path) {
                if (character == '\\') {
                    character = '/';
                }
            }
            while (path.starts_with("./")) {
                path.erase(0, 2);
            }
            if (!path.empty() && path.front() != '/') {
                path.insert(path.begin(), '/');
            }
            return path;
        }

        bool ReadLocalExtraLength(std::ifstream &file, const std::uint64_t localOffset, std::uint16_t &extraLength) {
            file.clear();
            file.seekg(static_cast<std::streamoff>(localOffset + 28));
            unsigned char bytes[2] = {};
            file.read(reinterpret_cast<char *>(bytes), 2);
            if (!file) {
                return false;
            }
            extraLength = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
            return true;
        }
    }

    void ApplyApkZipAlignment(std::vector<ApkFileEntry> &entries, const std::string &apkPath) {
        if (entries.empty() || apkPath.empty()) {
            return;
        }

        mz_zip_archive zip = {};
        if (!mz_zip_reader_init_file(&zip, apkPath.c_str(), 0)) {
            return;
        }

        std::ifstream file(apkPath, std::ios::binary);
        std::unordered_map<std::string, std::pair<std::uint32_t, bool>> layouts;
        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        layouts.reserve(count);
        for (mz_uint index = 0; index < count; ++index) {
            mz_zip_archive_file_stat stat = {};
            if (!mz_zip_reader_file_stat(&zip, index, &stat) || stat.m_is_directory) {
                continue;
            }
            std::uint16_t extraLength = 0;
            if (!ReadLocalExtraLength(file, stat.m_local_header_ofs, extraLength)) {
                continue;
            }
            const std::uint64_t dataOffset = stat.m_local_header_ofs + 30 + std::char_traits<char>::length(stat.m_filename) + extraLength;
            layouts.emplace(ZipLookupKey(stat.m_filename), std::pair{ZipDataAlignment(dataOffset), stat.m_method == 0});
        }
        mz_zip_reader_end(&zip);

        for (auto &entry: entries) {
            const auto found = layouts.find(ZipLookupKey(entry.Path));
            if (found == layouts.end()) {
                continue;
            }
            entry.Alignment = found->second.first;
            entry.Stored = found->second.second;
        }
    }

    namespace {
        bool AdvanceApkLoad(const ApkLoadProgressFn onProgress, void *user, const float progress, const int stage) {
            return onProgress == nullptr || onProgress(user, progress, stage);
        }
    }

    ApkReport LoadApkReport(
        const SdkInfo &sdk,
        const std::string &apkPath,
        const ApkLoadProgressFn onProgress,
        void *progressUser
    ) {
        ApkReport report;
        report.ApkPath = apkPath;
        if (sdk.ApkAnalyzerPath.empty() || !std::filesystem::exists(sdk.ApkAnalyzerPath)) {
            report.ToolMissing = true;
            return report;
        }
        if (apkPath.empty()) {
            return report;
        }

        const auto step = [&](const float progress, const int stage) {
            if (AdvanceApkLoad(onProgress, progressUser, progress, stage)) {
                return true;
            }
            report.Cancelled = true;
            return false;
        };

        if (!step(0.02F, 1)) {
            return report;
        }
        report.Summary = QueryApkSummary(sdk, apkPath);
        if (!step(0.08F, 1)) {
            return report;
        }
        report.FileSize = QueryApkFileSize(sdk, apkPath);
        if (!step(0.14F, 1)) {
            return report;
        }
        report.DownloadSize = QueryApkDownloadSize(sdk, apkPath);
        if (!step(0.55F, 1)) {
            return report;
        }
        report.Debuggable = QueryApkDebuggable(sdk, apkPath);

        if (!step(0.75F, 2)) {
            return report;
        }
        report.Files = QueryApkFiles(sdk, apkPath);
        if (report.Files.Ok) {
            ApplyApkZipAlignment(report.Files.Value, apkPath);
        }
        step(1.0F, 2);
        return report;
    }

    ApkAnalyzerQuery<std::vector<ApkCompareEntry>> LoadApkCompare(
        const SdkInfo &sdk,
        const std::string &baselineApkPath,
        const std::string &apkPath
    ) {
        return QueryApkCompare(sdk, baselineApkPath, apkPath);
    }
}

//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_APK_ANALYZER_WORK_H
#define COREDECK_APK_ANALYZER_WORK_H

#include <atomic>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "core/apk_analyzer.h"

namespace CoreDeck::Gui {
    struct ApkAnalyzerWork {
        std::atomic<bool> Busy{false};
        std::atomic<float> Progress{0.0F};
        std::atomic<int> Stage{1};
        std::atomic<bool> Cancel{false};
        std::atomic<std::uint32_t> LoadEpoch{0};
        std::string PendingPath;
        std::future<Core::ApkReport> Future;
        bool HasReport = false;
        Core::ApkReport Report;

        std::atomic<bool> CompareBusy{false};
        std::string ComparePath;
        std::future<Core::ApkAnalyzerQuery<std::vector<Core::ApkCompareEntry>>> CompareFuture;
        bool HasCompare = false;
        Core::ApkAnalyzerQuery<std::vector<Core::ApkCompareEntry>> Compare;

        std::string SelectedPath;
        std::uint32_t ContentEpoch = 0;

        char TreeFilter[128] = {};

        std::vector<std::string> RecentApks;
        bool RecentLoaded = false;
        std::vector<std::string> PendingDrops;
    };
}

#endif // COREDECK_APK_ANALYZER_WORK_H

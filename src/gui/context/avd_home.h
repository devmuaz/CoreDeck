//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_AVD_HOME_H
#define COREDECK_AVD_HOME_H

#include <atomic>
#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/avd_manager.h"
#include "core/options.h"
#include "async_work.h"
#include "../widgets/log_viewer.h"

namespace CoreDeck::Gui {
    enum class AvdSortMode : uint8_t {
        Name,
        ApiLevel,
        Device,
    };

    struct StorageScanResult {
        std::uintmax_t TotalAvdSize = 0;
        std::uintmax_t SystemImagesSize = 0;
    };

    struct Catalog {
        std::vector<std::string> AvdNames;
        std::vector<Core::AvdInfo> Avds;
        int SelectedAvd = -1;
        int PreviousSelectedAvd = -1;
        std::unordered_map<std::string, std::vector<Core::EmulatorOption>> PerAvdOptions;

        char SearchFilter[128] = {};
        AvdSortMode SortMode = AvdSortMode::Name;
        bool SortAscending = true;
        std::vector<int> FilteredIndices;
    };

    struct Logs {
        std::unordered_map<std::string, LogViewerState> PerAvdView;
    };

    struct Jobs {
        AsyncWork<void> AvdCreation, AvdDeletion, AvdWipe;
    };

    struct DiskUsage {
        std::unordered_map<std::string, std::uintmax_t> PerAvdCache;
        StorageScanResult LastScan;
        std::atomic<bool> Loading{false};
        bool Ready = false;
        std::future<StorageScanResult> Future;
    };
}

#endif // COREDECK_AVD_HOME_H

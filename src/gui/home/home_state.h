//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_GUI_HOME_HOME_STATE_H
#define COREDECK_GUI_HOME_HOME_STATE_H

#include <atomic>
#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/avd/avd_manager.h"
#include "core/avd/emulator_options.h"
#include "gui/shell/async_work.h"
#include "gui/widgets/log_viewer.h"

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

#endif // COREDECK_GUI_HOME_HOME_STATE_H

//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_LOG_VIEWER_H
#define COREDECK_LOG_VIEWER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/log_buffer.h"

namespace CoreDeck::Gui {
    struct LogViewerState {
        std::string Search;
        int ActiveMatchIndex = 0;
        bool UseRegex = false;
        std::string Filter;
        bool FilterUseRegex = false;
        bool PendingScroll = false;
        bool PendingFocus = false;
        bool PendingTop = false;
        int PendingSyncFrames = 0;
        bool Paused = false;
        std::vector<std::string> PausedLines;
        std::uint64_t TextStamp = 0;
    };

    struct LogViewerOptions {
        const char *EmptyText = nullptr;
        bool AutoScroll = true;
        bool AllowPause = false;
        bool Live = false;
        float MinBodyHeight = 0.0F;
    };

    void LogViewer(
        const char *id,
        const std::shared_ptr<Platform::LogBuffer> &log,
        LogViewerState &state,
        const LogViewerOptions &options = {}
    );

    void LogViewer(
        const char *id,
        std::string &text,
        bool &present,
        LogViewerState &state,
        const LogViewerOptions &options = {}
    );
}

#endif // COREDECK_LOG_VIEWER_H

//
// Created by AbdulMuaz Aqeel on 27/04/2026.
//

#ifndef COREDECK_CRASH_REPORTER_H
#define COREDECK_CRASH_REPORTER_H

#include <string_view>

namespace CoreDeck::CrashReporter {
    enum class Level {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    bool Init();

    void Shutdown();

    bool IsEnabled();

    void CaptureMessage(Level level, std::string_view message);

    void CaptureException(std::string_view type, std::string_view message);

    void AddBreadcrumb(std::string_view category, std::string_view message);
}

#endif //COREDECK_CRASH_REPORTER_H
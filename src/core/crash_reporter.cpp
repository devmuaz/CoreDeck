//
// Created by AbdulMuaz Aqeel on 27/04/2026.
//

#include "crash_reporter.h"

#ifdef COREDECK_SENTRY_DSN

#include <sentry.h>
#include <string>

#include "paths.h"

namespace CoreDeck::CrashReporter {
    static sentry_level_t ToSentryLevel(const Level level) {
        switch (level) {
            case Level::Debug: return SENTRY_LEVEL_DEBUG;
            case Level::Info: return SENTRY_LEVEL_INFO;
            case Level::Warning: return SENTRY_LEVEL_WARNING;
            case Level::Error: return SENTRY_LEVEL_ERROR;
            case Level::Fatal: return SENTRY_LEVEL_FATAL;
        }
        return SENTRY_LEVEL_INFO;
    }

    static std::string ToString(const std::string_view sv) {
        return std::string(sv);
    }

    bool Init() {
        sentry_options_t *opts = sentry_options_new();
        sentry_options_set_dsn(opts, COREDECK_SENTRY_DSN);
        sentry_options_set_release(opts, "coredeck@" COREDECK_VERSION);
        sentry_options_set_dist(opts, COREDECK_BUILD_NUMBER);
        sentry_options_set_database_path(opts, Paths::GetAppConfigPath("sentry-db").c_str());

        const char *handlerName =
#ifdef _WIN32
                "crashpad_handler.exe";
#else
                "crashpad_handler";
#endif
        const std::string handlerPath = Paths::JoinPaths(
            {Paths::GetExecutableDirectory(), handlerName}
        );
        sentry_options_set_handler_path(opts, handlerPath.c_str());

        return sentry_init(opts) == 0;
    }

    void Shutdown() {
        sentry_close();
    }

    bool IsEnabled() { return true; }

    void CaptureMessage(const Level level, const std::string_view message) {
        sentry_capture_event(sentry_value_new_message_event(
                ToSentryLevel(level),
                nullptr,
                ToString(message).c_str()
            )
        );
    }

    void CaptureException(const std::string_view type, const std::string_view message) {
        const sentry_value_t event = sentry_value_new_event();
        const sentry_value_t exc = sentry_value_new_exception(
            ToString(type).c_str(),
            ToString(message).c_str()
        );
        sentry_value_set_stacktrace(exc, nullptr, 0);
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);
    }

    void AddBreadcrumb(const std::string_view category, const std::string_view message) {
        const sentry_value_t crumb = sentry_value_new_breadcrumb(
            nullptr,
            ToString(message).c_str()
        );
        sentry_value_set_by_key(crumb, "category", sentry_value_new_string(ToString(category).c_str()));
        sentry_add_breadcrumb(crumb);
    }
}

#else

namespace CoreDeck::CrashReporter {
    bool Init() { return false; }

    void Shutdown() {
    }

    bool IsEnabled() { return false; }

    void CaptureMessage(Level, std::string_view) {
    }

    void CaptureException(std::string_view, std::string_view) {
    }

    void AddBreadcrumb(std::string_view, std::string_view) {
    }
}

#endif

//
// Created by AbdulMuaz Aqeel on 20/04/2026.
//

#ifndef COREDECK_LOG_H
#define COREDECK_LOG_H

#include <iostream>
#include <string_view>

namespace CoreDeck {
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
    };

    namespace Log {
        inline LogLevel MinLevel =
#ifdef NDEBUG
                LogLevel::Warning;
#else
                LogLevel::Debug;
#endif

        constexpr const char *LevelTag(const LogLevel level) {
            switch (level) {
                case LogLevel::Debug: return "[DEBUG]";
                case LogLevel::Info: return "[INFO] ";
                case LogLevel::Warning: return "[WARN] ";
                case LogLevel::Error: return "[ERROR]";
            }
            return "[UNKNOWN]";
        }

        template<typename... Args>
        void Write(const LogLevel level, Args &&... args) {
            if (level < MinLevel) return;
            auto &out = (level >= LogLevel::Error) ? std::cerr : std::cout;
            out << LevelTag(level) << " ";
            (out << ... << std::forward<Args>(args));
            out << std::endl;
        }

        template<typename... Args>
        void Debug(Args &&... args) { Write(LogLevel::Debug, std::forward<Args>(args)...); }

        template<typename... Args>
        void Info(Args &&... args) { Write(LogLevel::Info, std::forward<Args>(args)...); }

        template<typename... Args>
        void Warn(Args &&... args) { Write(LogLevel::Warning, std::forward<Args>(args)...); }

        template<typename... Args>
        void Error(Args &&... args) { Write(LogLevel::Error, std::forward<Args>(args)...); }
    }
}

#endif //COREDECK_LOG_H

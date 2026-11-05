//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_LOGCAT_H
#define COREDECK_ADB_LOGCAT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/sdk.h"
#include "platform/log_buffer.h"
#include "platform/process.h"

namespace CoreDeck::Core {
    enum class AdbLogPriority : uint8_t {
        Verbose,
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
    };

    struct AdbLogcatOptions {
        std::vector<std::string> Buffers{"main", "system", "crash"};
        AdbLogPriority MinimumPriority = AdbLogPriority::Verbose;
        int TailLines = 200;
        std::size_t MaxLines = 20000;
    };

    struct AdbLogcatStatus {
        bool Running = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Error;
    };

    std::vector<std::string> BuildAdbLogcatArguments(
        const std::string &serial,
        const AdbLogcatOptions &options
    );

    class AdbLogcatSession {
    public:
        AdbLogcatSession(const AdbLogcatSession &) = delete;
        AdbLogcatSession(AdbLogcatSession &&) = delete;
        AdbLogcatSession &operator=(const AdbLogcatSession &) = delete;
        AdbLogcatSession &operator=(AdbLogcatSession &&) = delete;

        AdbLogcatSession();

        ~AdbLogcatSession();

        bool Start(const SdkInfo &sdk, const std::string &serial, const AdbLogcatOptions &options);

        void Stop();

        AdbLogcatStatus Status() const;

        std::shared_ptr<Platform::LogBuffer> Log() const;

    private:
        void m_ReadStdout();

        void m_ReadStderr();

        mutable std::mutex m_Mutex;
        Platform::PipedProcess m_Process;
        std::shared_ptr<Platform::LogBuffer> m_Log;
        std::thread m_StdoutThread;
        std::thread m_StderrThread;
        std::atomic<bool> m_StopRequested{false};
        std::atomic<bool> m_Running{false};
        bool m_Started = false;
        int m_ExitCode = -1;
        std::string m_Error;
    };
}

#endif // COREDECK_ADB_LOGCAT_H

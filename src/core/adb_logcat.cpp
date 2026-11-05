//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "core/adb_logcat.h"

#include <chrono>
#include <cstddef>
#include <vector>

#include "core/adb_client.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        constexpr std::size_t MAX_ERROR_SIZE = static_cast<const std::size_t>(64 * 1024);

        template<typename LineFn>
        void ReadLines(
            const int fd,
            const std::size_t chunk,
            const std::atomic<bool> &stop,
            LineFn onLine
        ) {
            std::vector<char> buffer(chunk);
            Platform::LineStreamDecoder decoder;
            while (!stop.load()) {
                const Platform::PipeReadResult read = Platform::ReadProcessPipe(fd, buffer.data(), buffer.size());
                if (read.Status == Platform::PipeReadStatus::Data) {
                    decoder.Push(buffer.data(), read.Size, onLine);
                    continue;
                }
                if (read.Status == Platform::PipeReadStatus::Closed ||
                    read.Status == Platform::PipeReadStatus::Error) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            decoder.Flush(onLine);
        }

        char PriorityCode(const AdbLogPriority priority) {
            switch (priority) {
                case AdbLogPriority::Verbose:
                    return 'V';
                case AdbLogPriority::Debug:
                    return 'D';
                case AdbLogPriority::Info:
                    return 'I';
                case AdbLogPriority::Warning:
                    return 'W';
                case AdbLogPriority::Error:
                    return 'E';
                case AdbLogPriority::Fatal:
                    return 'F';
            }
            return 'V';
        }
    }

    std::vector<std::string> BuildAdbLogcatArguments(
        const std::string &serial,
        const AdbLogcatOptions &options
    ) {
        std::vector<std::string> command{"logcat", "-v", "threadtime"};
        for (const std::string &buffer: options.Buffers) {
            if (!buffer.empty()) {
                command.emplace_back("-b");
                command.push_back(buffer);
            }
        }
        if (options.TailLines > 0) {
            command.emplace_back("-T");
            command.push_back(std::to_string(options.TailLines));
        }
        command.push_back(std::string("*:") + PriorityCode(options.MinimumPriority));
        return BuildAdbArguments(serial, command);
    }

    AdbLogcatSession::AdbLogcatSession()
        : m_Log(std::make_shared<Platform::LogBuffer>(AdbLogcatOptions{}.MaxLines)) {
    }

    AdbLogcatSession::~AdbLogcatSession() {
        Stop();
    }

    bool AdbLogcatSession::Start(
        const SdkInfo &sdk,
        const std::string &serial,
        const AdbLogcatOptions &options
    ) {
        Stop();
        {
            const std::scoped_lock lock(m_Mutex);
            m_Started = false;
            m_ExitCode = -1;
            m_Error.clear();
        }
        if (sdk.AdbPath.empty() || serial.empty()) {
            const std::scoped_lock lock(m_Mutex);
            m_Error = sdk.AdbPath.empty() ? "adb is unavailable" : "No device is selected";
            return false;
        }

        const Platform::PipedProcess process = Platform::SpawnProcessWithPipes(
            sdk.AdbPath,
            BuildAdbLogcatArguments(serial, options),
            sdk.ToolEnv
        );
        if (!process) {
            const std::scoped_lock lock(m_Mutex);
            m_Error = "Could not start adb logcat";
            return false;
        }

        {
            const std::scoped_lock lock(m_Mutex);
            m_Process = process;
            m_Log = std::make_shared<Platform::LogBuffer>(options.MaxLines);
            m_Error.clear();
            m_ExitCode = -1;
            m_Started = true;
        }
        m_StopRequested.store(false);
        m_Running.store(true);
        m_StdoutThread = std::thread(&AdbLogcatSession::m_ReadStdout, this);
        m_StderrThread = std::thread(&AdbLogcatSession::m_ReadStderr, this);
        return true;
    }

    void AdbLogcatSession::Stop() {
        m_StopRequested.store(true);
        Platform::PipedProcess process;
        {
            const std::scoped_lock lock(m_Mutex);
            process = m_Process;
        }

        if (process) {
            Platform::InterruptProcess(process.Pid);
        }
        if (m_StdoutThread.joinable()) {
            m_StdoutThread.join();
        }
        if (m_StderrThread.joinable()) {
            m_StderrThread.join();
        }

        if (process) {
            Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 500);
            if (!exit.Exited) {
                Platform::InterruptProcess(process.Pid);
                exit = Platform::WaitForProcessResult(process, 500);
            }
            Platform::ClosePipedProcess(process);
            const std::scoped_lock lock(m_Mutex);
            m_ExitCode = exit.ExitCode;
            m_Process = {};
        }
        m_Running.store(false);
    }

    AdbLogcatStatus AdbLogcatSession::Status() const {
        const std::scoped_lock lock(m_Mutex);
        return {
            .Running = m_Running.load(),
            .Started = m_Started,
            .ExitCode = m_ExitCode,
            .Error = m_Error,
        };
    }

    std::shared_ptr<Platform::LogBuffer> AdbLogcatSession::Log() const {
        const std::scoped_lock lock(m_Mutex);
        return m_Log;
    }

    void AdbLogcatSession::m_ReadStdout() {
        int fd = -1;
        std::shared_ptr<Platform::LogBuffer> log;
        {
            const std::scoped_lock lock(m_Mutex);
            fd = m_Process.StdoutFd;
            log = m_Log;
        }

        ReadLines(fd, 4096, m_StopRequested, [&log](const std::string &line) {
            if (!line.empty()) {
                log->Push(line);
            }
        });
        m_Running.store(false);
    }

    void AdbLogcatSession::m_ReadStderr() {
        int fd = -1;
        {
            const std::scoped_lock lock(m_Mutex);
            fd = m_Process.StderrFd;
        }

        ReadLines(fd, 1024, m_StopRequested, [this](const std::string &line) {
            if (line.empty()) {
                return;
            }
            const std::scoped_lock lock(m_Mutex);
            if (!m_Error.empty()) {
                m_Error.push_back('\n');
            }
            Platform::AppendLimited(m_Error, line, MAX_ERROR_SIZE);
        });
    }
}

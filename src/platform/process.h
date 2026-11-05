//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef COREDECK_PROCESS_H
#define COREDECK_PROCESS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "env.h"

#if defined(_WIN32)
#include <windows.h>
using ProcessId = DWORD;
#else
#include <sys/types.h>
using ProcessId = pid_t;
#endif

namespace CoreDeck::Platform {
    enum class PipeReadStatus : uint8_t {
        Data,
        WouldBlock,
        Closed,
        Error,
    };

    struct PipeReadResult {
        PipeReadStatus Status = PipeReadStatus::Error;
        std::size_t Size = 0;
    };

    struct PipedProcess {
        ProcessId Pid = 0;
        std::uintptr_t NativeHandle = 0;
        int StdoutFd = -1;
        int StderrFd = -1;

        explicit operator bool() const {
#ifdef _WIN32
            return Pid != 0;
#else
            return Pid > 0;
#endif
        }
    };

    struct ProcessExitResult {
        bool Exited = false;
        int ExitCode = -1;
    };

    class LineStreamDecoder {
    public:
        void Push(const char *data, std::size_t size, const std::function<void(const std::string &)> &onLine);

        void Flush(const std::function<void(const std::string &)> &onLine);

    private:
        std::string m_Partial;
    };

    struct CommandResult {
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
    };

    std::string RunCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData = "",
        const EnvVars &extraEnv = {}
    );

    CommandResult RunCommandArgsResult(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData = "",
        const EnvVars &extraEnv = {}
    );

    void StreamCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData,
        const std::function<void(const std::string &)> &onLine,
        const EnvVars &extraEnv = {}
    );

    ProcessId SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd);

    PipedProcess SpawnProcessWithPipes(
        const std::string &path,
        const std::vector<std::string> &args,
        const EnvVars &extraEnv = {},
        bool separateStderr = true
    );

    PipeReadResult ReadProcessPipe(int fd, char *buffer, std::size_t size);

    void CloseProcessPipe(int fd);

    bool KillProcess(ProcessId pid);

    bool InterruptProcess(ProcessId pid);

    bool TerminateProcessTree(ProcessId pid);

    bool WaitForProcessExit(ProcessId pid, int timeoutMs);

    ProcessExitResult WaitForProcessResult(ProcessId pid, int timeoutMs);

    ProcessExitResult WaitForProcessResult(PipedProcess &process, int timeoutMs);

    void ClosePipedProcess(PipedProcess &process);

    bool IsProcessRunning(ProcessId pid);

    void CollectProcessTreePids(ProcessId pid, std::vector<ProcessId> &out);
}

#endif // COREDECK_PROCESS_H

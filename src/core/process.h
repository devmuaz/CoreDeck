//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef EMU_LAUNCHER_PROCESS_H
#define EMU_LAUNCHER_PROCESS_H

#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
using ProcessId = DWORD;
#else
#include <sys/types.h>
using ProcessId = pid_t;
#endif

namespace CoreDeck {
    std::string RunCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData = ""
    );

    void StreamCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData,
        const std::function<void(const std::string &)> &onLine
    );

    ProcessId SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd);

    bool KillProcess(ProcessId pid);

    bool TerminateProcessTree(ProcessId pid);

    bool WaitForProcessExit(ProcessId pid, int timeoutMs);

    bool IsProcessRunning(ProcessId pid);
}

#endif //EMU_LAUNCHER_PROCESS_H

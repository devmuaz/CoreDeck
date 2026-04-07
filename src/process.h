//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#ifndef EMU_LAUNCHER_PROCESS_H
#define EMU_LAUNCHER_PROCESS_H

#include <string>
#include <vector>
#include <sys/types.h>

namespace Emu {
    std::string RunCommand(const std::string &cmd);

    pid_t SpawnProcess(const std::string &path, const std::vector<std::string> &args);

    pid_t SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd);

    bool KillProcess(pid_t pid);

    bool IsProcessRunning(pid_t pid);
}

#endif //EMU_LAUNCHER_PROCESS_H

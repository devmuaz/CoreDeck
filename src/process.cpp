//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "process.h"
#include <array>
#include <csignal>
#include <string>
#include <unistd.h>
#include <sys/fcntl.h>

namespace Emu {
    std::string RunCommand(const std::string &cmd) {
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return "";
        }

        std::string result;
        std::array<char, 256> buffer{};

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        pclose(pipe);
        return result;
    }

    pid_t SpawnProcess(const std::string &path, const std::vector<std::string> &args) {
        const pid_t pid = fork();

        if (pid < 0) {
            return -1;
        }

        if (pid == 0) {
            std::vector<const char *> argv;
            argv.push_back(path.c_str());

            for (const auto &arg: args) {
                argv.push_back(arg.c_str());
            }
            argv.push_back(nullptr);

            if (const int devnull = open("/dev/null", O_WRONLY); devnull != -1) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            execvp(path.c_str(), const_cast<char *const*>(argv.data()));

            _exit(1);
        }

        return pid;
    }

    pid_t SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd) {
        int pipeFd[2];
        if (pipe(pipeFd) == -1) {
            return -1;
        }

        const pid_t pid = fork();

        if (pid < 0) {
            close(pipeFd[0]);
            close(pipeFd[1]);
            return -1;
        }

        if (pid == 0) {
            close(pipeFd[0]);

            dup2(pipeFd[1], STDOUT_FILENO);
            dup2(pipeFd[1], STDERR_FILENO);
            close(pipeFd[1]);

            std::vector<const char *> argv;
            argv.push_back(path.c_str());
            for (const auto &arg: args) {
                argv.push_back(arg.c_str());
            }
            argv.push_back(nullptr);

            execvp(path.c_str(), const_cast<char * const*>(argv.data()));
            _exit(1);
        }

        close(pipeFd[1]);
        outputFd = pipeFd[0];

        const int flags = fcntl(outputFd, F_GETFL, 0);
        fcntl(outputFd, F_SETFL, flags | O_NONBLOCK);

        return pid;
    }

    bool KillProcess(const pid_t pid) {
        if (pid <= 0) return false;

        if (kill(pid, SIGTERM) == 0) {
            int status;
            if (const pid_t result = waitpid(pid, &status, WNOHANG); result == 0) {
                usleep(500000);
                waitpid(pid, &status, WNOHANG);
            }
            return true;
        }
        return false;
    }

    bool IsProcessRunning(const pid_t pid) {
        if (pid <= 0) return false;

        int status;
        if (const pid_t result = waitpid(pid, &status, WNOHANG); result == 0) {
            return true;
        }
        return false;
    }
}

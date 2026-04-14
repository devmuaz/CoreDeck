//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "process.h"
#include "paths.h"
#include <array>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#else
#include <csignal>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#endif

namespace CoreDeck {
    std::string RunCommand(const std::string &cmd) {
#ifdef _WIN32
        FILE *pipe = _popen(cmd.c_str(), "r");
        if (!pipe) return "";

        std::string result;
        std::array<char, 256> buffer{};

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        _pclose(pipe);
        return result;
#else
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";

        std::string result;
        std::array<char, 256> buffer{};

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        pclose(pipe);
        return result;
#endif
    }

    ProcessId SpawnProcess(const std::string &path, const std::vector<std::string> &args) {
#ifdef _WIN32
        std::string cmdLine = "\"" + path + "\"";
        for (const auto &arg: args) {
            cmdLine += " \"" + arg + "\"";
        }

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;

        const std::string nullDevice = Paths::GetNullDevice();
        HANDLE hNull = CreateFileA(nullDevice.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hNull != INVALID_HANDLE_VALUE) {
            si.hStdOutput = hNull;
            si.hStdError = hNull;
        }

        PROCESS_INFORMATION pi = {};

        if (!CreateProcessA(nullptr, const_cast<char *>(cmdLine.c_str()), nullptr, nullptr,
                            TRUE, 0, nullptr, nullptr, &si, &pi)) {
            if (hNull != INVALID_HANDLE_VALUE) CloseHandle(hNull);
            return 0;
        }

        CloseHandle(pi.hThread);
        if (hNull != INVALID_HANDLE_VALUE) CloseHandle(hNull);

        return pi.dwProcessId;
#else
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

            const std::string nullDevice = Paths::GetNullDevice();
            if (const int devnull = open(nullDevice.c_str(), O_WRONLY); devnull != -1) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            execvp(path.c_str(), const_cast<char *const*>(argv.data()));

            _exit(1);
        }

        return pid;
#endif
    }

    ProcessId SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd) {
#ifdef _WIN32
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return 0;
        }

        if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return 0;
        }

        std::string cmdLine = "\"" + path + "\"";
        for (const auto &arg: args) {
            cmdLine += " \"" + arg + "\"";
        }

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};

        if (!CreateProcessA(nullptr, const_cast<char *>(cmdLine.c_str()), nullptr, nullptr,
                            TRUE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return 0;
        }

        CloseHandle(pi.hThread);
        CloseHandle(hWritePipe);

        outputFd = _open_osfhandle(reinterpret_cast<intptr_t>(hReadPipe), _O_RDONLY);
        if (outputFd == -1) {
            CloseHandle(hReadPipe);
            return 0;
        }

        return pi.dwProcessId;
#else
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
#endif
    }

    bool KillProcess(const ProcessId pid) {
#ifdef _WIN32
        if (pid == 0) return false;

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) return false;

        DWORD exitCode;
        if (!GetExitCodeProcess(hProcess, &exitCode)) {
            CloseHandle(hProcess);
            return false;
        }

        if (exitCode != STILL_ACTIVE) {
            CloseHandle(hProcess);
            return true;
        }

        const bool result = TerminateProcess(hProcess, 1);
        if (result) {
            WaitForSingleObject(hProcess, 500);
        }

        CloseHandle(hProcess);
        return result;
#else
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
#endif
    }

    bool IsProcessRunning(const ProcessId pid) {
#ifdef _WIN32
        if (pid == 0) return false;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) return false;

        DWORD exitCode;
        const bool success = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);

        return success && exitCode == STILL_ACTIVE;
#else
        if (pid <= 0) return false;

        int status;
        if (const pid_t result = waitpid(pid, &status, WNOHANG); result == 0) {
            return true;
        }
        return false;
#endif
    }
}

//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "process.h"
#include <algorithm>
#include <array>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#else
#include <csignal>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#endif

namespace CoreDeck {
#ifdef _WIN32
    static std::string QuoteArg(const std::string &arg) {
        if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos) return arg;
        std::string out = "\"";
        for (size_t i = 0; i < arg.size(); ++i) {
            size_t backslashes = 0;
            while (i < arg.size() && arg[i] == '\\') {
                ++backslashes;
                ++i;
            }
            if (i == arg.size()) {
                out.append(backslashes * 2, '\\');
                break;
            }
            if (arg[i] == '"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('"');
            } else {
                out.append(backslashes, '\\');
                out.push_back(arg[i]);
            }
        }
        out.push_back('"');
        return out;
    }

    static bool IsBatchFile(const std::string &path) {
        if (path.size() < 4) return false;
        std::string ext = path.substr(path.size() - 4);
        std::ranges::transform(ext, ext.begin(), ::tolower);
        return ext == ".bat" || ext == ".cmd";
    }

    static std::string BuildCommandLine(const std::string &path, const std::vector<std::string> &args) {
        std::string cmd = QuoteArg(path);
        for (const auto &arg: args) {
            cmd.push_back(' ');
            cmd += QuoteArg(arg);
        }
        if (IsBatchFile(path)) return "cmd.exe /S /C \"" + cmd + "\"";
        return cmd;
    }
#endif

    void StreamCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData,
        const std::function<void(const std::string &)> &onLine
    ) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hOutR = nullptr, hOutW = nullptr;
        HANDLE hInR = nullptr, hInW = nullptr;
        if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) return;
        SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
        if (!CreatePipe(&hInR, &hInW, &sa, 0)) {
            CloseHandle(hOutR);
            CloseHandle(hOutW);
            return;
        }
        SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);

        std::string cmdLine = BuildCommandLine(path, args);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hInR;
        si.hStdOutput = hOutW;
        si.hStdError = hOutW;

        PROCESS_INFORMATION pi = {};
        if (!CreateProcessA(nullptr, const_cast<char *>(cmdLine.c_str()), nullptr, nullptr,
                            TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hOutR);
            CloseHandle(hOutW);
            CloseHandle(hInR);
            CloseHandle(hInW);
            return;
        }

        CloseHandle(hOutW);
        CloseHandle(hInR);

        if (!stdinData.empty()) {
            DWORD written = 0;
            WriteFile(hInW, stdinData.data(), static_cast<DWORD>(stdinData.size()), &written, nullptr);
        }
        CloseHandle(hInW);

        std::string partial;
        std::array<char, 512> buf{};
        DWORD read = 0;
        while (ReadFile(hOutR, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr) && read > 0) {
            partial.append(buf.data(), read);
            std::size_t pos;
            while ((pos = partial.find_first_of("\n\r")) != std::string::npos) {
                if (auto line = partial.substr(0, pos); !line.empty() && onLine) onLine(line);
                auto next = partial.find_first_not_of("\n\r", pos);
                partial = (next == std::string::npos) ? std::string() : partial.substr(next);
            }
        }
        if (!partial.empty() && onLine) onLine(partial);

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hOutR);
#else
        int outPipe[2];
        int inPipe[2];
        if (pipe(outPipe) == -1) return;
        if (pipe(inPipe) == -1) {
            close(outPipe[0]);
            close(outPipe[1]);
            return;
        }

        const pid_t pid = fork();
        if (pid < 0) {
            close(outPipe[0]);
            close(outPipe[1]);
            close(inPipe[0]);
            close(inPipe[1]);
            return;
        }

        if (pid == 0) {
            close(outPipe[0]);
            close(inPipe[1]);
            dup2(outPipe[1], STDOUT_FILENO);
            dup2(outPipe[1], STDERR_FILENO);
            dup2(inPipe[0], STDIN_FILENO);
            close(outPipe[1]);
            close(inPipe[0]);

            std::vector<const char *> argv;
            argv.push_back(path.c_str());
            for (const auto &a: args) argv.push_back(a.c_str());
            argv.push_back(nullptr);
            execvp(path.c_str(), const_cast<char *const *>(argv.data()));
            _exit(127);
        }

        close(outPipe[1]);
        close(inPipe[0]);

        if (!stdinData.empty()) {
            [[maybe_unused]] ssize_t w = write(inPipe[1], stdinData.data(), stdinData.size());
        }
        close(inPipe[1]);

        std::string partial;
        std::array<char, 512> buf{};
        ssize_t r;
        while ((r = read(outPipe[0], buf.data(), buf.size())) > 0) {
            partial.append(buf.data(), r);
            std::size_t pos;
            while ((pos = partial.find_first_of("\n\r")) != std::string::npos) {
                if (auto line = partial.substr(0, pos); !line.empty() && onLine) onLine(line);
                auto next = partial.find_first_not_of("\n\r", pos);
                partial = (next == std::string::npos) ? std::string() : partial.substr(next);
            }
        }
        if (!partial.empty() && onLine) onLine(partial);

        close(outPipe[0]);
        int status;
        waitpid(pid, &status, 0);
#endif
    }

    std::string RunCommandArgs(const std::string &path,
                               const std::vector<std::string> &args,
                               const std::string &stdinData) {
        std::string out;
        StreamCommandArgs(path, args, stdinData, [&out](const std::string &line) {
            out += line;
            out.push_back('\n');
        });
        return out;
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

        std::string cmdLine = BuildCommandLine(path, args);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};

        if (!CreateProcessA(nullptr, const_cast<char *>(cmdLine.c_str()), nullptr, nullptr,
                            TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            return 0;
        }

        CloseHandle(pi.hThread);
        CloseHandle(hWritePipe);

        DWORD pipeMode = PIPE_NOWAIT;
        SetNamedPipeHandleState(hReadPipe, &pipeMode, nullptr, nullptr);

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
            setpgid(0, 0);
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

    bool WaitForProcessExit(const ProcessId pid, const int timeoutMs) {
#ifdef _WIN32
        if (pid == 0) return false;
        HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) return true;
        const DWORD r = WaitForSingleObject(hProcess, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        CloseHandle(hProcess);
        return r == WAIT_OBJECT_0;
#else
        if (pid <= 0) return false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true) {
            int status;
            const pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid || r == -1) return true;
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
#endif
    }

    bool TerminateProcessTree(const ProcessId pid) {
#ifdef _WIN32
        if (pid == 0) return false;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            std::vector<DWORD> children;
            PROCESSENTRY32 pe = {};
            pe.dwSize = sizeof(pe);
            if (Process32First(snap, &pe)) {
                do {
                    if (pe.th32ParentProcessID == pid) children.push_back(pe.th32ProcessID);
                } while (Process32Next(snap, &pe));
            }
            CloseHandle(snap);
            for (const DWORD child: children) TerminateProcessTree(child);
        }

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (hProcess == nullptr) return true;
        const bool ok = TerminateProcess(hProcess, 1) != 0;
        if (ok) WaitForSingleObject(hProcess, 2000);
        CloseHandle(hProcess);
        return ok;
#else
        if (pid <= 0) return false;
        if (kill(-pid, SIGKILL) == 0) {
            WaitForProcessExit(pid, 2000);
            return true;
        }
        const bool ok = kill(pid, SIGKILL) == 0;
        if (ok) WaitForProcessExit(pid, 2000);
        return ok;
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

//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include "process.h"
#include <array>
#include <cctype>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>

extern char **environ; // NOLINT(readability-identifier-naming)
#endif

namespace CoreDeck::Platform {
    namespace {
#ifdef _WIN32
        std::vector<char> BuildEnvironmentBlock(const EnvVars &extra) {
            std::vector<std::pair<std::string, std::string>> entries;

            if (LPCH strings = GetEnvironmentStringsA()) {
                for (LPCH p = strings; *p != '\0';) {
                    std::string entry(p);
                    p += entry.size() + 1;
                    if (entry.empty()) {
                        continue;
                    }
                    // Drive-letter pseudo-vars start with '=' (e.g. "=C:=C:\\"),
                    // so search for the separator past the first character.
                    const auto eq = entry.find('=', 1);
                    if (eq == std::string::npos) {
                        entries.emplace_back(entry, std::string());
                    } else {
                        entries.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
                    }
                }
                FreeEnvironmentStringsA(strings);
            }

            const auto iEquals = [](const std::string &a, const std::string &b) {
                if (a.size() != b.size()) {
                    return false;
                }
                for (size_t i = 0; i < a.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                        return false;
                    }
                }
                return true;
            };

            for (const auto &ev: extra) {
                bool found = false;
                for (auto &e: entries) {
                    if (iEquals(e.first, ev.Name)) {
                        e.second = ev.Value;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    entries.emplace_back(ev.Name, ev.Value);
                }
            }

            std::vector<char> block;
            for (const auto &e: entries) {
                const std::string line = e.first + "=" + e.second;
                block.insert(block.end(), line.begin(), line.end());
                block.push_back('\0');
            }
            block.push_back('\0');
            return block;
        }

        std::string QuoteArg(const std::string &arg) {
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

        bool IsBatchFile(const std::string &path) {
            if (path.size() < 4) return false;
            std::string ext = path.substr(path.size() - 4);
            std::ranges::transform(ext, ext.begin(), ::tolower);
            return ext == ".bat" || ext == ".cmd";
        }

        std::string BuildCommandLine(const std::string &path, const std::vector<std::string> &args) {
            std::string cmd = QuoteArg(path);
            for (const auto &arg: args) {
                cmd.push_back(' ');
                cmd += QuoteArg(arg);
            }
            if (IsBatchFile(path)) return "cmd.exe /S /C \"" + cmd + "\"";
            return cmd;
        }
#else
        // execve and execvp require char *const[]. Keep mutable copies so the
        // pointers refer to non-const character data.
        std::vector<char *> MutableArgv(
            std::vector<std::string> &storage,
            const std::string &path,
            const std::vector<std::string> &args
        ) {
            storage.clear();
            storage.reserve(args.size() + 1);
            storage.push_back(path);
            storage.insert(storage.end(), args.begin(), args.end());
            std::vector<char *> argv;
            argv.reserve(storage.size() + 1);
            for (std::string &arg: storage) {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            return argv;
        }

        std::vector<std::string> BuildMergedEnv(const EnvVars &extra) {
            std::vector<std::string> merged;
            std::vector<bool> used(extra.size(), false);

            for (char *const *e = environ; e != nullptr && *e != nullptr; ++e) {
                std::string entry(*e);
                const auto eq = entry.find('=');
                const std::string name = (eq == std::string::npos) ? entry : entry.substr(0, eq);

                bool overridden = false;
                for (size_t i = 0; i < extra.size(); ++i) {
                    if (extra.at(i).Name == name) {
                        merged.push_back(extra.at(i).Name + "=" + extra.at(i).Value);
                        used.at(i) = true;
                        overridden = true;
                        break;
                    }
                }
                if (!overridden) {
                    merged.push_back(std::move(entry));
                }
            }

            for (size_t i = 0; i < extra.size(); ++i) {
                if (!used.at(i)) {
                    merged.push_back(extra.at(i).Name + "=" + extra.at(i).Value);
                }
            }
            return merged;
        }

        struct ProcessEnvironment {
            std::vector<std::string> Entries;
            std::vector<char *> Pointers;
        };

        ProcessEnvironment MakeProcessEnvironment(const EnvVars &extra) {
            ProcessEnvironment environment;
            if (extra.empty()) {
                return environment;
            }
            environment.Entries = BuildMergedEnv(extra);
            environment.Pointers.reserve(environment.Entries.size() + 1);
            for (std::string &entry: environment.Entries) {
                environment.Pointers.push_back(entry.data());
            }
            environment.Pointers.push_back(nullptr);
            return environment;
        }
#endif

        struct CommandSpawnStatus {
            bool Started = false;
            int ExitCode = -1;
        };

        // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
        CommandSpawnStatus CaptureCommand(
            const std::string &path,
            const std::vector<std::string> &args,
            const std::string &stdinData,
            const std::function<void(const std::string &)> &onLine,
            const EnvVars &extraEnv
        ) {
            CommandSpawnStatus spawn;
#ifdef _WIN32
            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE hOutR = nullptr, hOutW = nullptr;
            HANDLE hInR = nullptr, hInW = nullptr;
            if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) return {};
            SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
            if (!CreatePipe(&hInR, &hInW, &sa, 0)) {
                CloseHandle(hOutR);
                CloseHandle(hOutW);
                return {};
            }
            SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);

            std::string cmdLine = BuildCommandLine(path, args);

            std::vector<char> envBlock;
            LPVOID envPtr = nullptr;
            if (!extraEnv.empty()) {
                envBlock = BuildEnvironmentBlock(extraEnv);
                envPtr = envBlock.data();
            }

            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = hInR;
            si.hStdOutput = hOutW;
            si.hStdError = hOutW;

            PROCESS_INFORMATION pi = {};
            if (!CreateProcessA(nullptr, const_cast<char *>(cmdLine.c_str()), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, envPtr, nullptr, &si, &pi)) {
                CloseHandle(hOutR);
                CloseHandle(hOutW);
                CloseHandle(hInR);
                CloseHandle(hInW);
                return {};
            }
            spawn.Started = true;

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

            if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0) {
                DWORD code = 0;
                if (GetExitCodeProcess(pi.hProcess, &code)) {
                    spawn.ExitCode = static_cast<int>(code);
                }
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hOutR);
#else
            int outPipe[2];
            int inPipe[2];
            if (pipe(outPipe) == -1) {
                return {};
            }
            if (pipe(inPipe) == -1) {
                close(outPipe[0]);
                close(outPipe[1]);
                return {};
            }

            const ProcessEnvironment environment = MakeProcessEnvironment(extraEnv);

            const pid_t pid = fork();
            if (pid < 0) {
                close(outPipe[0]);
                close(outPipe[1]);
                close(inPipe[0]);
                close(inPipe[1]);
                return {};
            }

            if (pid == 0) {
                close(outPipe[0]);
                close(inPipe[1]);
                dup2(outPipe[1], STDOUT_FILENO);
                dup2(outPipe[1], STDERR_FILENO);
                dup2(inPipe[0], STDIN_FILENO);
                close(outPipe[1]);
                close(inPipe[0]);

                std::vector<std::string> argvStorage;
                const std::vector<char *> argv = MutableArgv(argvStorage, path, args);
                if (!environment.Pointers.empty()) {
                    execve(path.c_str(), argv.data(), environment.Pointers.data());
                } else {
                    execvp(path.c_str(), argv.data());
                }
                _exit(127);
            }
            spawn.Started = true;

            close(outPipe[1]);
            close(inPipe[0]);

            if (!stdinData.empty()) {
                [[maybe_unused]] const ssize_t w = write(inPipe[1], stdinData.data(), stdinData.size());
            }
            close(inPipe[1]);

            std::string partial;
            std::array<char, 512> buf{};
            ssize_t r = 0;
            while ((r = read(outPipe[0], buf.data(), buf.size())) > 0) {
                partial.append(buf.data(), r);
                std::size_t pos = 0;
                while ((pos = partial.find_first_of("\n\r")) != std::string::npos) {
                    if (const auto line = partial.substr(0, pos); !line.empty() && onLine) {
                        onLine(line);
                    }
                    const auto next = partial.find_first_not_of("\n\r", pos);
                    partial = (next == std::string::npos) ? std::string() : partial.substr(next);
                }
            }
            if (!partial.empty() && onLine) {
                onLine(partial);
            }

            close(outPipe[0]);
            int waitStatus = 0;
            pid_t waited = waitpid(pid, &waitStatus, 0);
            while (waited < 0 && errno == EINTR) {
                waited = waitpid(pid, &waitStatus, 0);
            }
            if (waited == pid) {
                if (WIFEXITED(waitStatus)) {
                    spawn.ExitCode = WEXITSTATUS(waitStatus);
                } else if (WIFSIGNALED(waitStatus)) {
                    spawn.ExitCode = 128 + WTERMSIG(waitStatus);
                }
            }
#endif
            return spawn;
        }
    }

    void LineStreamDecoder::Push(
        const char *data,
        const std::size_t size,
        const std::function<void(const std::string &)> &onLine
    ) {
        m_Partial.append(data, size);
        std::size_t start = 0;
        for (std::size_t i = 0; i < m_Partial.size(); ++i) {
            if (m_Partial.at(i) != '\n' && m_Partial.at(i) != '\r') {
                continue;
            }
            if (m_Partial.at(i) == '\r' && i + 1 == m_Partial.size()) {
                break;
            }
            if (onLine) {
                onLine(m_Partial.substr(start, i - start));
            }
            if (m_Partial.at(i) == '\r' && i + 1 < m_Partial.size() && m_Partial.at(i + 1) == '\n') {
                ++i;
            }
            start = i + 1;
        }
        if (start > 0) {
            m_Partial.erase(0, start);
        }
    }

    void LineStreamDecoder::Flush(const std::function<void(const std::string &)> &onLine) {
        if (!m_Partial.empty() && onLine) {
            if (m_Partial.back() == '\r') {
                onLine(m_Partial.substr(0, m_Partial.size() - 1));
            } else {
                onLine(m_Partial);
            }
        }
        m_Partial.clear();
    }

    void StreamCommandArgs(
        const std::string &path,
        const std::vector<std::string> &args,
        const std::string &stdinData,
        const std::function<void(const std::string &)> &onLine,
        const EnvVars &extraEnv
    ) {
        CaptureCommand(path, args, stdinData, onLine, extraEnv);
    }

    CommandResult RunCommandArgsResult(const std::string &path, const std::vector<std::string> &args, const std::string &stdinData, const EnvVars &extraEnv) {
        CommandResult result;
        const CommandSpawnStatus spawn = CaptureCommand(path, args, stdinData, [&result](const std::string &line) {
            result.Output += line;
            result.Output.push_back('\n'); }, extraEnv);
        result.Started = spawn.Started;
        result.ExitCode = spawn.ExitCode;
        return result;
    }

    std::string RunCommandArgs(const std::string &path, const std::vector<std::string> &args, const std::string &stdinData, const EnvVars &extraEnv) {
        return RunCommandArgsResult(path, args, stdinData, extraEnv).Output;
    }

    namespace {
#ifdef _WIN32
        PipedProcess SpawnPipedProcess(
            const std::string &path,
            const std::vector<std::string> &args,
            const EnvVars &extraEnv,
            const bool separateStderr
        ) {
            PipedProcess result;
            HANDLE hStdoutRead = nullptr;
            HANDLE hStdoutWrite = nullptr;
            HANDLE hStderrRead = nullptr;
            HANDLE hStderrWrite = nullptr;
            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
                return result;
            }
            if (!SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
                CloseHandle(hStdoutRead);
                CloseHandle(hStdoutWrite);
                return result;
            }
            if (separateStderr) {
                if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
                    CloseHandle(hStdoutRead);
                    CloseHandle(hStdoutWrite);
                    return result;
                }
                if (!SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0)) {
                    CloseHandle(hStdoutRead);
                    CloseHandle(hStdoutWrite);
                    CloseHandle(hStderrRead);
                    CloseHandle(hStderrWrite);
                    return result;
                }
            }

            std::string cmdLine = BuildCommandLine(path, args);
            std::vector<char> envBlock;
            LPVOID envPtr = nullptr;
            if (!extraEnv.empty()) {
                envBlock = BuildEnvironmentBlock(extraEnv);
                envPtr = envBlock.data();
            }

            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = hStdoutWrite;
            si.hStdError = separateStderr ? hStderrWrite : hStdoutWrite;

            PROCESS_INFORMATION pi = {};

            if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, envPtr, nullptr, &si, &pi)) {
                CloseHandle(hStdoutRead);
                CloseHandle(hStdoutWrite);
                if (hStderrRead != nullptr) CloseHandle(hStderrRead);
                if (hStderrWrite != nullptr) CloseHandle(hStderrWrite);
                return result;
            }

            CloseHandle(pi.hThread);
            CloseHandle(hStdoutWrite);
            if (hStderrWrite != nullptr) CloseHandle(hStderrWrite);

            DWORD pipeMode = PIPE_NOWAIT;
            SetNamedPipeHandleState(hStdoutRead, &pipeMode, nullptr, nullptr);
            if (hStderrRead != nullptr) SetNamedPipeHandleState(hStderrRead, &pipeMode, nullptr, nullptr);

            result.StdoutFd = _open_osfhandle(reinterpret_cast<intptr_t>(hStdoutRead), _O_RDONLY | _O_BINARY);
            if (result.StdoutFd == -1) {
                CloseHandle(hStdoutRead);
                if (hStderrRead != nullptr) CloseHandle(hStderrRead);
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                return {};
            }
            if (hStderrRead != nullptr) {
                result.StderrFd = _open_osfhandle(reinterpret_cast<intptr_t>(hStderrRead), _O_RDONLY | _O_BINARY);
                if (result.StderrFd == -1) {
                    _close(result.StdoutFd);
                    CloseHandle(hStderrRead);
                    TerminateProcess(pi.hProcess, 1);
                    CloseHandle(pi.hProcess);
                    return {};
                }
            }
            result.Pid = pi.dwProcessId;
            result.NativeHandle = reinterpret_cast<std::uintptr_t>(pi.hProcess);
            return result;
        }
#else
        void SetPipeNonBlocking(const int fd) {
            if (fd < 0) {
                return;
            }
            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        }

        PipedProcess SpawnPipedProcess(
            const std::string &path,
            const std::vector<std::string> &args,
            const EnvVars &extraEnv,
            const bool separateStderr
        ) {
            PipedProcess result;
            int stdoutPipe[2];
            int stderrPipe[2] = {-1, -1};
            if (pipe(stdoutPipe) == -1) {
                return result;
            }
            if (separateStderr && pipe(stderrPipe) == -1) {
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
                return result;
            }

            const ProcessEnvironment environment = MakeProcessEnvironment(extraEnv);
            const pid_t pid = fork();

            if (pid < 0) {
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
                if (stderrPipe[0] >= 0) close(stderrPipe[0]);
                if (stderrPipe[1] >= 0) close(stderrPipe[1]);
                return result;
            }

            if (pid == 0) {
                setpgid(0, 0);
                close(stdoutPipe[0]);
                if (stderrPipe[0] >= 0) close(stderrPipe[0]);

                dup2(stdoutPipe[1], STDOUT_FILENO);
                dup2(separateStderr ? stderrPipe[1] : stdoutPipe[1], STDERR_FILENO);
                close(stdoutPipe[1]);
                if (stderrPipe[1] >= 0) close(stderrPipe[1]);

                std::vector<std::string> argvStorage;
                const std::vector<char *> argv = MutableArgv(argvStorage, path, args);
                if (!environment.Pointers.empty()) {
                    execve(path.c_str(), argv.data(), environment.Pointers.data());
                } else {
                    execvp(path.c_str(), argv.data());
                }
                _exit(1);
            }

            close(stdoutPipe[1]);
            if (stderrPipe[1] >= 0) close(stderrPipe[1]);
            result.Pid = pid;
            result.StdoutFd = stdoutPipe[0];
            result.StderrFd = stderrPipe[0];
            SetPipeNonBlocking(result.StdoutFd);
            SetPipeNonBlocking(result.StderrFd);
            return result;
        }
#endif
    }

    PipedProcess SpawnProcessWithPipes(
        const std::string &path,
        const std::vector<std::string> &args,
        const EnvVars &extraEnv,
        const bool separateStderr
    ) {
        return SpawnPipedProcess(path, args, extraEnv, separateStderr);
    }

    ProcessId SpawnProcessWithPipe(const std::string &path, const std::vector<std::string> &args, int &outputFd) {
        const PipedProcess process = SpawnProcessWithPipes(path, args, {}, false);
        outputFd = process.StdoutFd;
#ifdef _WIN32
        if (process.NativeHandle != 0) {
            CloseHandle(reinterpret_cast<HANDLE>(process.NativeHandle));
        }
#endif
        return process.Pid;
    }

    PipeReadResult ReadProcessPipe(const int fd, char *buffer, const std::size_t size) {
        if (fd < 0 || buffer == nullptr || size == 0) {
            return {};
        }
#ifdef _WIN32
        const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
        DWORD bytesRead = 0;
        if (ReadFile(handle, buffer, static_cast<DWORD>(size), &bytesRead, nullptr)) {
            return bytesRead > 0
                       ? PipeReadResult{.Status = PipeReadStatus::Data, .Size = bytesRead}
                       : PipeReadResult{.Status = PipeReadStatus::WouldBlock};
        }
        const DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
            return {.Status = PipeReadStatus::Closed};
        }
        if (error == ERROR_NO_DATA) {
            return {.Status = PipeReadStatus::WouldBlock};
        }
        return {};
#else
        const ssize_t bytesRead = read(fd, buffer, size);
        if (bytesRead > 0) {
            return {.Status = PipeReadStatus::Data, .Size = static_cast<std::size_t>(bytesRead)};
        }
        if (bytesRead == 0) {
            return {.Status = PipeReadStatus::Closed};
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return {.Status = PipeReadStatus::WouldBlock};
        }
        return {};
#endif
    }

    void CloseProcessPipe(const int fd) {
        if (fd < 0) {
            return;
        }
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
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
        if (pid <= 0) {
            return false;
        }

        if (kill(pid, SIGTERM) == 0) {
            int status = 0;
            if (const pid_t result = waitpid(pid, &status, WNOHANG); result == 0) {
                usleep(500000);
                waitpid(pid, &status, WNOHANG);
            }
            return true;
        }
        return false;
#endif
    }

    bool InterruptProcess(const ProcessId pid) {
#ifdef _WIN32
        if (pid == 0) {
            return false;
        }
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == nullptr) {
            return false;
        }
        const bool result = TerminateProcess(hProcess, 1) != FALSE;
        CloseHandle(hProcess);
        return result;
#else
        if (pid <= 0) {
            return false;
        }
        return kill(pid, SIGKILL) == 0;
#endif
    }

    ProcessExitResult WaitForProcessResult(const ProcessId pid, const int timeoutMs) {
        ProcessExitResult result;
#ifdef _WIN32
        if (pid == 0) return result;
        HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) {
            result.Exited = true;
            return result;
        }
        const DWORD r = WaitForSingleObject(hProcess, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        if (r == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            result.Exited = true;
            if (GetExitCodeProcess(hProcess, &exitCode)) {
                result.ExitCode = static_cast<int>(exitCode);
            }
        }
        CloseHandle(hProcess);
        return result;
#else
        if (pid <= 0) {
            return result;
        }
        const auto deadline = timeoutMs < 0
                                  ? std::chrono::steady_clock::time_point::max()
                                  : std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true) {
            int status = 0;
            const pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) {
                result.Exited = true;
                if (WIFEXITED(status)) {
                    result.ExitCode = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    result.ExitCode = 128 + WTERMSIG(status);
                }
                return result;
            }
            if (r == -1) {
                result.Exited = true;
                return result;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
#endif
    }

    bool WaitForProcessExit(const ProcessId pid, const int timeoutMs) {
        return WaitForProcessResult(pid, timeoutMs).Exited;
    }

    // Windows clears NativeHandle after closing the process handle. POSIX only reads the pid.
    ProcessExitResult WaitForProcessResult(PipedProcess &process, const int timeoutMs) { // NOLINT(misc-const-correctness)
#ifdef _WIN32
        ProcessExitResult result;
        if (process.NativeHandle == 0) {
            return WaitForProcessResult(process.Pid, timeoutMs);
        }
        const auto handle = reinterpret_cast<HANDLE>(process.NativeHandle);
        const DWORD wait = WaitForSingleObject(handle, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        if (wait == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            result.Exited = true;
            if (GetExitCodeProcess(handle, &exitCode)) {
                result.ExitCode = static_cast<int>(exitCode);
            }
            CloseHandle(handle);
            process.NativeHandle = 0;
        }
        return result;
#else
        return WaitForProcessResult(process.Pid, timeoutMs);
#endif
    }

    void ClosePipedProcess(PipedProcess &process) {
        CloseProcessPipe(process.StdoutFd);
        CloseProcessPipe(process.StderrFd);
        process.StdoutFd = -1;
        process.StderrFd = -1;
#ifdef _WIN32
        if (process.NativeHandle != 0) {
            CloseHandle(reinterpret_cast<HANDLE>(process.NativeHandle));
            process.NativeHandle = 0;
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
        if (pid <= 0) {
            return false;
        }
        if (kill(-pid, SIGKILL) == 0) {
            WaitForProcessExit(pid, 2000);
            return true;
        }
        const bool ok = kill(pid, SIGKILL) == 0;
        if (ok) {
            WaitForProcessExit(pid, 2000);
        }
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
        if (pid <= 0) {
            return false;
        }

        int status = 0;
        if (const pid_t result = waitpid(pid, &status, WNOHANG); result == 0) {
            return true;
        }
        return false;
#endif
    }

    void CollectProcessTreePids(const ProcessId pid, std::vector<ProcessId> &out) {
        if (pid == 0) {
            return;
        }

#ifdef _WIN32
        out.push_back(pid);

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) {
            return;
        }

        std::vector<DWORD> children;
        PROCESSENTRY32 pe = {};
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (pe.th32ParentProcessID == pid) {
                    children.push_back(pe.th32ProcessID);
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);

        for (const DWORD child: children) {
            CollectProcessTreePids(child, out);
        }
#else
        out.push_back(pid);
#endif
    }
}

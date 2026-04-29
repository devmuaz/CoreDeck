//
// Created by AbdulMuaz Aqeel on 03/04/2026.
//

#include <ranges>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <array>
#include <chrono>
#include <cerrno>

#include "emulator.h"
#include "process.h"
#include "emulator_console.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace CoreDeck {
    EmulatorManager::EmulatorManager(SdkInfo sdk)
        : m_Sdk(std::move(sdk)) {
    }

    EmulatorManager::~EmulatorManager() {
        std::vector<std::thread> pendingStops; {
            std::lock_guard lock(m_Mutex);
            for (auto &instance: m_Instances | std::views::values) {
                if (instance.StopThread.joinable()) {
                    pendingStops.push_back(std::move(instance.StopThread));
                }
            }
        }
        for (auto &t: pendingStops) t.join();

        std::lock_guard lock(m_Mutex);
        for (auto &instance: m_Instances | std::views::values) {
            if (instance.IsRunning) {
                if (instance.ConsolePort > 0) {
                    EmulatorConsole::SendKill(instance.ConsolePort, 1000);
                }
                if (!WaitForProcessExit(instance.Pid, 2000)) {
                    TerminateProcessTree(instance.Pid);
                }
                instance.IsRunning = false;
            }

            if (instance.StopRequested) {
                instance.StopRequested->store(true);
            }

            if (instance.ReaderThread.joinable()) {
                instance.ReaderThread.join();
            }
        }
    }

    bool EmulatorManager::Launch(const std::string &avdName, const std::vector<std::string> &args) { {
            std::lock_guard lock(m_Mutex);
            if (const auto it = m_Instances.find(avdName); it != m_Instances.end() && it->second.IsRunning) {
                return false;
            }
        }

        const int consolePort = EmulatorConsole::FindFreePort();
        std::vector<std::string> finalArgs = args;
        if (consolePort > 0) {
            finalArgs.emplace_back("-port");
            finalArgs.emplace_back(std::to_string(consolePort));
        }

        int outputFd = -1;
        const ProcessId pid = SpawnProcessWithPipe(m_Sdk.EmulatorPath, finalArgs, outputFd);

#ifdef _WIN32
        if (pid == 0) return false;
#else
        if (pid <= 0) return false;
#endif

        {
            auto log = std::make_shared<LogBuffer>();
            auto stopFlag = std::make_shared<std::atomic<bool> >(false);
            std::thread reader([outputFd, log, stopFlag] {
                std::array<char, 1024> buf{};
                std::string partial;

                auto flushLines = [&](const char *data, const std::size_t n) {
                    partial.append(data, n);
                    std::size_t pos;
                    while ((pos = partial.find('\n')) != std::string::npos) {
                        if (auto line = partial.substr(0, pos); !line.empty()) {
                            log->Push(line);
                        }
                        partial.erase(0, pos + 1);
                    }
                };

                while (!stopFlag->load()) {
#ifdef _WIN32
                    const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(outputFd));
                    DWORD nRead = 0;
                    if (ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &nRead, nullptr)) {
                        if (nRead > 0) {
                            flushLines(buf.data(), nRead);
                        } else {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }
                    } else {
                        const DWORD err = GetLastError();
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF) break;
                        if (err == ERROR_NO_DATA) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        } else {
                            break;
                        }
                    }
#else
                    if (const ssize_t n = read(outputFd, buf.data(), buf.size()); n > 0) {
                        flushLines(buf.data(), static_cast<std::size_t>(n));
                    } else if (n == 0) {
                        break;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    } else {
                        break;
                    }
#endif
                }

                if (!partial.empty()) {
                    log->Push(partial);
                }

#ifdef _WIN32
                _close(outputFd);
#else
                close(outputFd);
#endif
            });

            std::thread oldStopThread;
            std::thread oldReaderThread; {
                std::lock_guard lock(m_Mutex);
                if (const auto existing = m_Instances.find(avdName); existing != m_Instances.end()) {
                    if (existing->second.StopRequested) {
                        existing->second.StopRequested->store(true);
                    }
                    oldStopThread = std::move(existing->second.StopThread);
                    oldReaderThread = std::move(existing->second.ReaderThread);
                    m_Instances.erase(existing);
                }
            }
            if (oldStopThread.joinable()) oldStopThread.join();
            if (oldReaderThread.joinable()) oldReaderThread.join();

            std::lock_guard lock(m_Mutex);

            EmulatorInstance instance;
            instance.AvdName = avdName;
            instance.Pid = pid;
            instance.ConsolePort = consolePort;
            instance.IsRunning = true;
            instance.Log = log;
            instance.ReaderThread = std::move(reader);
            instance.StopRequested = stopFlag;
            m_Instances[avdName] = std::move(instance);
        }

        return true;
    }

    bool EmulatorManager::Stop(const std::string &avdName) {
        ProcessId pid;
        int consolePort;
        std::shared_ptr<std::atomic<bool> > stopFlag;
        std::thread readerThread;
        std::thread oldStopThread; {
            std::lock_guard lock(m_Mutex);
            const auto it = m_Instances.find(avdName);
            if (it == m_Instances.end() || !it->second.IsRunning || it->second.Stopping) {
                return false;
            }
            pid = it->second.Pid;
            consolePort = it->second.ConsolePort;
            stopFlag = it->second.StopRequested;
            readerThread = std::move(it->second.ReaderThread);
            oldStopThread = std::move(it->second.StopThread);
            it->second.Stopping = true;
        }
        if (oldStopThread.joinable()) oldStopThread.join();

        std::thread worker(
            [this, avdName, pid, consolePort, stopFlag,reader = std::move(readerThread)]() mutable {
                bool exited = false;
                if (consolePort > 0 && EmulatorConsole::SendKill(consolePort)) {
                    exited = WaitForProcessExit(pid, 5000);
                }
                if (!exited) {
                    KillProcess(pid);
                    exited = WaitForProcessExit(pid, 2000);
                }
                if (!exited) {
                    TerminateProcessTree(pid);
                }

                if (stopFlag) stopFlag->store(true);
                if (reader.joinable()) reader.join();

                std::lock_guard lock(m_Mutex);
                if (const auto it = m_Instances.find(avdName); it != m_Instances.end()) {
                    it->second.IsRunning = false;
                    it->second.Stopping = false;
                }
            }
        );

        std::lock_guard lock(m_Mutex);
        if (const auto it = m_Instances.find(avdName); it != m_Instances.end()) {
            it->second.StopThread = std::move(worker);
        } else {
            worker.detach();
        }
        return true;
    }

    bool EmulatorManager::IsStopping(const std::string &avdName) {
        std::lock_guard lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) return false;
        return it->second.Stopping;
    }

    bool EmulatorManager::IsRunning(const std::string &avdName) {
        std::lock_guard lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) return false;
        return it->second.IsRunning;
    }

    std::shared_ptr<LogBuffer> EmulatorManager::GetLog(const std::string &avdName) {
        std::lock_guard lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) return nullptr;
        return it->second.Log;
    }

    void EmulatorManager::Update() {
        std::lock_guard lock(m_Mutex);
        for (auto &instance: m_Instances | std::views::values) {
            if (instance.IsRunning) {
                if (!IsProcessRunning(instance.Pid)) {
                    instance.IsRunning = false;
                }
            }
        }
    }

    void EmulatorManager::SetSdk(SdkInfo sdk) {
        std::lock_guard lock(m_Mutex);
        m_Sdk = std::move(sdk);
    }
}

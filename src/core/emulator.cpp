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

#include "core/emulator.h"
#include "platform/process.h"
#include "core/emulator_console.h"

namespace CoreDeck::Core {
    namespace {
        bool WaitForConsoleUnavailable(const int port, const int timeoutMs) {
            if (port <= 0) {
                return true;
            }

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (std::chrono::steady_clock::now() < deadline) {
                if (!Core::EmulatorConsole::IsAvailable(port, 200)) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return !Core::EmulatorConsole::IsAvailable(port, 200);
        }


        void RunOutputReader(
            const int outputFd,
            const std::shared_ptr<Platform::LogBuffer> &log,
            const std::shared_ptr<std::atomic<bool>> &stopFlag
        ) {
            std::array<char, 1024> buf{};
            Platform::LineStreamDecoder decoder;
            const auto pushLine = [&log](const std::string &line) {
                if (!line.empty()) {
                    log->Push(line);
                }
            };

            while (!stopFlag->load()) {
                const Platform::PipeReadResult read = Platform::ReadProcessPipe(outputFd, buf.data(), buf.size());
                if (read.Status == Platform::PipeReadStatus::Data) {
                    decoder.Push(buf.data(), read.Size, pushLine);
                    continue;
                }
                if (read.Status == Platform::PipeReadStatus::Closed ||
                    read.Status == Platform::PipeReadStatus::Error) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            decoder.Flush(pushLine);
            Platform::CloseProcessPipe(outputFd);
        }
    }

    EmulatorManager::EmulatorManager(SdkInfo sdk)
        : m_Sdk(std::move(sdk)) {
        m_Stats.Start();
    }

    EmulatorManager::~EmulatorManager() {
        m_Stats.Stop();
        std::vector<std::thread> pendingStops;
        {
            const std::scoped_lock lock(m_Mutex);
            for (auto &instance: m_Instances | std::views::values) {
                if (instance.StopThread.joinable()) {
                    pendingStops.push_back(std::move(instance.StopThread));
                }
            }
        }
        for (auto &thread: pendingStops) {
            thread.join();
        }

        const std::scoped_lock lock(m_Mutex);
        for (auto &instance: m_Instances | std::views::values) {
            if (instance.IsRunning) {
                bool exited = false;
                if (instance.ConsolePort > 0 && Core::EmulatorConsole::SendKill(instance.ConsolePort, 1000)) {
                    exited = WaitForConsoleUnavailable(instance.ConsolePort, 5000);
                }
                if (!exited) {
                    Platform::KillProcess(instance.Pid);
                    exited = instance.ConsolePort > 0
                                 ? WaitForConsoleUnavailable(instance.ConsolePort, 2000)
                                 : Platform::WaitForProcessExit(instance.Pid, 2000);
                }
                if (!exited) {
                    Platform::TerminateProcessTree(instance.Pid);
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

    void EmulatorManager::m_EvictExistingInstance(const std::string &avdName) {
        std::thread oldStopThread;
        std::thread oldReaderThread;
        {
            const std::scoped_lock lock(m_Mutex);
            if (const auto existing = m_Instances.find(avdName); existing != m_Instances.end()) {
                if (existing->second.StopRequested) {
                    existing->second.StopRequested->store(true);
                }
                oldStopThread = std::move(existing->second.StopThread);
                oldReaderThread = std::move(existing->second.ReaderThread);
                m_Instances.erase(existing);
            }
        }
        if (oldStopThread.joinable()) {
            oldStopThread.join();
        }
        if (oldReaderThread.joinable()) {
            oldReaderThread.join();
        }
    }

    bool EmulatorManager::Launch(const std::string &avdName, const std::vector<std::string> &args) {
        {
            const std::scoped_lock lock(m_Mutex);
            if (const auto it = m_Instances.find(avdName); it != m_Instances.end() && it->second.IsRunning) {
                return false;
            }
        }

        const int consolePort = Core::EmulatorConsole::FindFreePort();
        std::vector<std::string> finalArgs = args;
        if (consolePort > 0) {
            finalArgs.emplace_back("-port");
            finalArgs.emplace_back(std::to_string(consolePort));
        }

        int outputFd = -1;
        const ProcessId pid = Platform::SpawnProcessWithPipe(m_Sdk.EmulatorPath, finalArgs, outputFd);

#ifdef _WIN32
        if (pid == 0) {
            return false;
        }
#else
        if (pid <= 0) {
            return false;
        }
#endif

        auto log = std::make_shared<Platform::LogBuffer>();
        auto stopFlag = std::make_shared<std::atomic<bool>>(false);
        std::thread reader(RunOutputReader, outputFd, log, stopFlag);

        m_EvictExistingInstance(avdName);
        {
            const std::scoped_lock lock(m_Mutex);

            EmulatorInstance instance;
            instance.AvdName = avdName;
            instance.Pid = pid;
            instance.ConsolePort = consolePort;
            instance.IsRunning = true;
            instance.Log = std::move(log);
            instance.ReaderThread = std::move(reader);
            instance.StopRequested = std::move(stopFlag);
            m_Instances[avdName] = std::move(instance);
        }

        m_Stats.Track(pid);
        return true;
    }

    bool EmulatorManager::Stop(const std::string &avdName) {
        ProcessId pid = 0;
        int consolePort = 0;
        std::shared_ptr<std::atomic<bool>> stopFlag;
        std::thread readerThread;
        std::thread oldStopThread;
        {
            const std::scoped_lock lock(m_Mutex);
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
        if (oldStopThread.joinable()) {
            oldStopThread.join();
        }

        std::thread worker(
            [this, avdName, pid, consolePort, stopFlag, reader = std::move(readerThread)]() mutable {
                bool exited = false;
                if (consolePort > 0 && Core::EmulatorConsole::SendKill(consolePort)) {
                    exited = WaitForConsoleUnavailable(consolePort, 10000);
                }
                if (!exited) {
                    Platform::KillProcess(pid);
                    exited = consolePort > 0
                                 ? WaitForConsoleUnavailable(consolePort, 3000)
                                 : Platform::WaitForProcessExit(pid, 2000);
                }
                if (!exited) {
                    Platform::TerminateProcessTree(pid);
                    exited = consolePort > 0
                                 ? WaitForConsoleUnavailable(consolePort, 3000)
                                 : Platform::WaitForProcessExit(pid, 2000);
                }

                if (exited) {
                    if (stopFlag) {
                        stopFlag->store(true);
                    }
                    if (reader.joinable()) {
                        reader.join();
                    }
                    m_Stats.Untrack(pid);
                }
                const bool handOff = !exited && reader.joinable();
                {
                    const std::scoped_lock lock(m_Mutex);
                    if (const auto it = m_Instances.find(avdName); it != m_Instances.end()) {
                        it->second.IsRunning = !exited;
                        it->second.Stopping = false;
                        if (handOff) {
                            it->second.ReaderThread = std::move(reader);
                        }
                    } else if (handOff) {
                        reader.detach();
                    }
                }
            }
        );

        const std::scoped_lock lock(m_Mutex);
        if (const auto it = m_Instances.find(avdName); it != m_Instances.end()) {
            it->second.StopThread = std::move(worker);
        } else {
            worker.detach();
        }
        return true;
    }

    bool EmulatorManager::IsStopping(const std::string &avdName) const {
        const std::scoped_lock lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) {
            return false;
        }
        return it->second.Stopping;
    }

    bool EmulatorManager::IsRunning(const std::string &avdName) const {
        const std::scoped_lock lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) {
            return false;
        }
        return it->second.IsRunning;
    }

    std::shared_ptr<Platform::LogBuffer> EmulatorManager::GetLog(const std::string &avdName) {
        const std::scoped_lock lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end()) {
            return nullptr;
        }
        return it->second.Log;
    }

    ProcessId EmulatorManager::GetPid(const std::string &avdName) const {
        const std::scoped_lock lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end() || !it->second.IsRunning) {
            return 0;
        }
        return it->second.Pid;
    }

    std::string EmulatorManager::GetAvdNameForConsolePort(const int consolePort) const {
        const std::scoped_lock lock(m_Mutex);
        for (const auto &instance: m_Instances | std::views::values) {
            if (instance.IsRunning && instance.ConsolePort == consolePort) {
                return instance.AvdName;
            }
        }
        return {};
    }

    void EmulatorManager::Update() {
        std::vector<ProcessId> toUntrack;
        {
            const std::scoped_lock lock(m_Mutex);
            for (auto &instance: m_Instances | std::views::values) {
                if (instance.IsRunning && !Platform::IsProcessRunning(instance.Pid)) {
                    instance.IsRunning = instance.ConsolePort > 0 &&
                                         Core::EmulatorConsole::IsAvailable(instance.ConsolePort, 25);
                    if (!instance.IsRunning) {
                        toUntrack.push_back(instance.Pid);
                    }
                }
            }
        }
        for (const ProcessId pid: toUntrack) {
            m_Stats.Untrack(pid);
        }
    }

    void EmulatorManager::SetSdk(SdkInfo sdk) {
        const std::scoped_lock lock(m_Mutex);
        m_Sdk = std::move(sdk);
    }
}

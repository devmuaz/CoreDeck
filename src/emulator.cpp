//
// Created by AbdulMuaz Aqeel on 03/04/2026.
//

#include "emulator.h"

#include <ranges>

#include "process.h"
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace CoreDeck {
    EmulatorManager::EmulatorManager(SdkInfo sdk)
        : m_Sdk(std::move(sdk)) {
    }

    bool EmulatorManager::Launch(const std::string &avdName, const std::vector<std::string> &args) { {
            std::lock_guard lock(m_Mutex);
            if (const auto it = m_Instances.find(avdName); it != m_Instances.end() && it->second.IsRunning) {
                return false;
            }
        }

        int outputFd = -1;
        const ProcessId pid = SpawnProcessWithPipe(m_Sdk.EmulatorPath, args, outputFd);

#ifdef _WIN32
        if (pid == 0) return false;
#else
        if (pid <= 0) return false;
#endif

        {
            auto log = std::make_shared<LogBuffer>();
            std::jthread reader([outputFd, log](const std::stop_token &st) {
                std::array<char, 1024> buf{};
                std::string partial;

                while (!st.stop_requested()) {
#ifdef _WIN32
                    if (const int n = _read(outputFd, buf.data(), buf.size()); n > 0) {
#else
                    if (const ssize_t n = read(outputFd, buf.data(), buf.size()); n > 0) {
#endif
                        partial.append(buf.data(), n);

                        std::size_t pos;
                        while ((pos = partial.find('\n')) != std::string::npos) {
                            if (auto line = partial.substr(0, pos); !line.empty()) {
                                log->Push(line);
                            }
                            partial.erase(0, pos + 1);
                        }
                    } else if (n == 0) {
                        break;
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
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

            std::lock_guard lock(m_Mutex);
            EmulatorInstance instance;
            instance.AvdName = avdName;
            instance.Pid = pid;
            instance.IsRunning = true;
            instance.Log = log;
            instance.ReaderThread = std::move(reader);
            m_Instances[avdName] = std::move(instance);
        }

        return true;
    }

    bool EmulatorManager::Stop(const std::string &avdName) {
        std::lock_guard lock(m_Mutex);
        const auto it = m_Instances.find(avdName);
        if (it == m_Instances.end() || !it->second.IsRunning) {
            return false;
        }

        const bool killed = KillProcess(it->second.Pid);
        if (killed) {
            it->second.IsRunning = false;
        }
        return killed;
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
}

//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "core/adb_service.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <thread>

#include "core/adb_client.h"
#include "platform/process.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        bool SameDevice(const AdbDevice &left, const AdbDevice &right) {
            return left.Serial == right.Serial &&
                   left.State == right.State &&
                   left.Product == right.Product &&
                   left.Model == right.Model &&
                   left.Device == right.Device &&
                   left.TransportId == right.TransportId;
        }

        bool SameDevices(const std::vector<AdbDevice> &left, const std::vector<AdbDevice> &right) {
            return left.size() == right.size() && std::ranges::equal(left, right, SameDevice);
        }

        bool PauseTracking(const std::atomic<bool> &stop, const std::chrono::milliseconds duration) {
            const auto deadline = std::chrono::steady_clock::now() + duration;
            while (!stop.load()) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            return true;
        }
    }

    AdbService::AdbService(SdkInfo sdk) : m_Sdk(std::move(sdk)) {
    }

    AdbService::~AdbService() {
        StopTracking();
        StopAllLogcat(true);
    }

    void AdbService::SetSdk(SdkInfo sdk) {
        const bool restartTracker = m_Tracking.load();
        StopTracking();
        StopAllLogcat(true);
        {
            const std::scoped_lock lock(m_Mutex);
            m_Sdk = std::move(sdk);
            m_Devices.clear();
            m_TrackerError.clear();
            ++m_Revision;
        }
        if (restartTracker) {
            StartTracking();
        }
    }

    SdkInfo AdbService::Sdk() const {
        const std::scoped_lock lock(m_Mutex);
        return m_Sdk;
    }

    void AdbService::StartTracking() {
        if (m_Tracking.exchange(true)) {
            return;
        }
        m_StopTracker.store(false);
        m_RefreshRequested.store(true);
        m_TrackerThread = std::thread(&AdbService::m_Track, this);
    }

    void AdbService::StopTracking() {
        m_StopTracker.store(true);
        const auto pid = static_cast<ProcessId>(m_TrackerPid.exchange(0));
        if (pid != 0) {
            Platform::InterruptProcess(pid);
        }
        if (m_TrackerThread.joinable()) {
            m_TrackerThread.join();
        }
        m_Tracking.store(false);
    }

    void AdbService::RequestRefresh() {
        m_RefreshRequested.store(true);
    }

    AdbDeviceSnapshot AdbService::Devices() const {
        const std::scoped_lock lock(m_Mutex);
        return {
            .Devices = m_Devices,
            .Revision = m_Revision,
            .Tracking = m_Tracking.load(),
            .Error = m_TrackerError,
        };
    }

    bool AdbService::StartLogcat(const std::string &serial, const AdbLogcatOptions &options) {
        StopLogcat(serial);
        auto session = std::make_unique<AdbLogcatSession>();
        if (!session->Start(Sdk(), serial, options)) {
            const std::scoped_lock lock(m_Mutex);
            m_Logcat[serial] = std::move(session);
            return false;
        }
        const std::scoped_lock lock(m_Mutex);
        m_Logcat[serial] = std::move(session);
        return true;
    }

    void AdbService::StopLogcat(const std::string &serial) {
        AdbLogcatSession *session = nullptr;
        {
            const std::scoped_lock lock(m_Mutex);
            const auto found = m_Logcat.find(serial);
            if (found == m_Logcat.end()) {
                return;
            }
            session = found->second.get();
        }
        session->Stop();
    }

    void AdbService::StopAllLogcat(const bool discard) {
        std::vector<AdbLogcatSession *> sessions;
        {
            const std::scoped_lock lock(m_Mutex);
            sessions.reserve(m_Logcat.size());
            for (const auto &[_, session]: m_Logcat) {
                sessions.push_back(session.get());
            }
        }
        for (AdbLogcatSession *session: sessions) {
            session->Stop();
        }
        if (discard) {
            const std::scoped_lock lock(m_Mutex);
            m_Logcat.clear();
        }
    }

    std::shared_ptr<Platform::LogBuffer> AdbService::LogcatBuffer(const std::string &serial) {
        const std::scoped_lock lock(m_Mutex);
        const auto found = m_Logcat.find(serial);
        return found == m_Logcat.end() ? nullptr : found->second->Log();
    }

    AdbLogcatStatus AdbService::LogcatStatus(const std::string &serial) {
        const std::scoped_lock lock(m_Mutex);
        const auto found = m_Logcat.find(serial);
        return found == m_Logcat.end() ? AdbLogcatStatus{} : found->second->Status();
    }

    void AdbService::m_PublishDevices(std::vector<AdbDevice> devices, std::string error) {
        std::ranges::sort(devices, {}, &AdbDevice::Serial);
        const std::scoped_lock lock(m_Mutex);
        if (SameDevices(m_Devices, devices) && m_TrackerError == error) {
            return;
        }
        m_Devices = std::move(devices);
        m_TrackerError = std::move(error);
        ++m_Revision;
    }

    void AdbService::m_Track() {
        using namespace std::chrono_literals;
        while (!m_StopTracker.load()) {
            const SdkInfo sdk = Sdk();
            if (sdk.AdbPath.empty()) {
                m_PublishDevices({}, "adb is unavailable");
                if (PauseTracking(m_StopTracker, 500ms)) {
                    break;
                }
                continue;
            }

            Platform::PipedProcess process = Platform::SpawnProcessWithPipes(
                sdk.AdbPath,
                {"track-devices", "-l"},
                sdk.ToolEnv
            );
            if (!process) {
                const AdbCommandResult fallback = RunAdb(sdk, {"devices", "-l"}, {}, &m_StopTracker);
                if (!m_StopTracker.load()) {
                    m_PublishDevices(
                        fallback.Ok() ? ParseAdbDevices(fallback.Stdout) : std::vector<AdbDevice>{},
                        fallback.Ok() ? std::string{} : fallback.Stderr
                    );
                }
                if (PauseTracking(m_StopTracker, 500ms)) {
                    break;
                }
                continue;
            }

            m_TrackerPid.store(static_cast<std::uint64_t>(process.Pid));

            std::array<char, 4096> stdoutBuffer{};
            std::array<char, 1024> stderrBuffer{};
            Platform::LineStreamDecoder decoder;
            std::vector<AdbDevice> pending;
            std::string trackerError;
            auto lastPoll = std::chrono::steady_clock::time_point::min();
            bool stdoutOpen = true;

            const auto handleLine = [this, &pending](const std::string &line) {
                if (line.empty()) {
                    m_PublishDevices(std::move(pending));
                    pending.clear();
                    return;
                }
                std::vector<AdbDevice> parsed = ParseAdbDevices(line);
                pending.insert(
                    pending.end(),
                    std::make_move_iterator(parsed.begin()),
                    std::make_move_iterator(parsed.end())
                );
            };

            while (!m_StopTracker.load() && stdoutOpen) {
                const Platform::PipeReadResult out = Platform::ReadProcessPipe(
                    process.StdoutFd,
                    stdoutBuffer.data(),
                    stdoutBuffer.size()
                );
                if (out.Status == Platform::PipeReadStatus::Data) {
                    decoder.Push(stdoutBuffer.data(), out.Size, handleLine);
                } else if (out.Status == Platform::PipeReadStatus::Closed || out.Status == Platform::PipeReadStatus::Error) {
                    stdoutOpen = false;
                }

                const Platform::PipeReadResult err = Platform::ReadProcessPipe(
                    process.StderrFd,
                    stderrBuffer.data(),
                    stderrBuffer.size()
                );
                if (err.Status == Platform::PipeReadStatus::Data) {
                    constexpr std::size_t MAX_ERROR_SIZE = static_cast<std::size_t>(64) * 1024;
                    Platform::AppendLimited(trackerError, std::string_view(stderrBuffer.data(), err.Size), MAX_ERROR_SIZE);
                }

                const auto now = std::chrono::steady_clock::now();
                if (!m_StopTracker.load() && (m_RefreshRequested.exchange(false) || now - lastPoll >= 2s)) {
                    const AdbCommandResult fallback = RunAdb(sdk, {"devices", "-l"}, {}, &m_StopTracker);
                    if (!m_StopTracker.load()) {
                        if (fallback.Ok()) {
                            m_PublishDevices(ParseAdbDevices(fallback.Stdout));
                        } else if (!fallback.Stderr.empty()) {
                            m_PublishDevices({}, fallback.Stderr);
                        }
                    }
                    lastPoll = now;
                }
                std::this_thread::sleep_for(25ms);
            }

            if (m_StopTracker.load()) {
                Platform::InterruptProcess(process.Pid);
            }
            Platform::WaitForProcessResult(process, m_StopTracker.load() ? 500 : 2000);
            m_TrackerPid.store(0);
            Platform::ClosePipedProcess(process);
            if (!trackerError.empty() && !m_StopTracker.load()) {
                const std::scoped_lock lock(m_Mutex);
                m_TrackerError = std::move(trackerError);
                ++m_Revision;
            }
            if (PauseTracking(m_StopTracker, 250ms)) {
                break;
            }
        }
        m_TrackerPid.store(0);
    }
}

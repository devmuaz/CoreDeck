//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_SERVICE_H
#define COREDECK_ADB_SERVICE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/adb_devices.h"
#include "core/adb_logcat.h"

namespace CoreDeck::Core {
    struct AdbDeviceSnapshot {
        std::vector<AdbDevice> Devices;
        std::uint64_t Revision = 0;
        bool Tracking = false;
        std::string Error;
    };

    class AdbService {
    public:
        AdbService(const AdbService &) = delete;
        AdbService(AdbService &&) = delete;
        AdbService &operator=(const AdbService &) = delete;
        AdbService &operator=(AdbService &&) = delete;

        explicit AdbService(SdkInfo sdk);

        ~AdbService();

        void SetSdk(SdkInfo sdk);

        SdkInfo Sdk() const;

        void StartTracking();

        void StopTracking();

        void RequestRefresh();

        AdbDeviceSnapshot Devices() const;

        bool StartLogcat(const std::string &serial, const AdbLogcatOptions &options = {});

        void StopLogcat(const std::string &serial);

        void StopAllLogcat(bool discard = false);

        std::shared_ptr<Platform::LogBuffer> LogcatBuffer(const std::string &serial);

        AdbLogcatStatus LogcatStatus(const std::string &serial);

    private:
        void m_Track();

        void m_PublishDevices(std::vector<AdbDevice> devices, std::string error = {});

        mutable std::mutex m_Mutex;
        SdkInfo m_Sdk;
        std::vector<AdbDevice> m_Devices;
        std::string m_TrackerError;
        std::uint64_t m_Revision = 0;
        std::unordered_map<std::string, std::unique_ptr<AdbLogcatSession>> m_Logcat;
        std::thread m_TrackerThread;
        std::atomic<bool> m_StopTracker{false};
        std::atomic<bool> m_Tracking{false};
        std::atomic<bool> m_RefreshRequested{false};
        std::atomic<std::uint64_t> m_TrackerPid{0};
    };
}

#endif // COREDECK_ADB_SERVICE_H

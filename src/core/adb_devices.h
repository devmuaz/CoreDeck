//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_DEVICES_H
#define COREDECK_ADB_DEVICES_H

#include <cstdint>
#include <string>
#include <vector>

namespace CoreDeck::Core {
    enum class AdbDeviceState : uint8_t {
        Device,
        Offline,
        Unauthorized,
        Recovery,
        Bootloader,
        Sideload,
        NoPermissions,
        Unknown,
    };

    struct AdbDevice {
        std::string Serial;
        AdbDeviceState State = AdbDeviceState::Unknown;
        std::string StateText;
        std::string Product;
        std::string Model;
        std::string Device;
        std::string TransportId;
        std::string AvdName;
        bool IsEmulator = false;
        int EmulatorConsolePort = 0;
    };

    struct AdbForward {
        std::string Serial;
        std::string Local;
        std::string Remote;
        bool Reverse = false;
    };

    AdbDeviceState ParseAdbDeviceState(const std::string &state);

    std::vector<AdbDevice> ParseAdbDevices(const std::string &output);

    std::vector<AdbForward> ParseAdbForwards(const std::string &output, bool reverse = false);
}

#endif // COREDECK_ADB_DEVICES_H

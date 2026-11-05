//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "core/adb_devices.h"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string_view>

#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        bool IsReverseHostId(const std::string &token) {
            constexpr std::string_view PREFIX = "host-";
            if (token == "host") {
                return true;
            }
            if (!token.starts_with(PREFIX) || token.size() == PREFIX.size()) {
                return false;
            }
            return std::ranges::all_of(token.substr(PREFIX.size()), [](const char character) {
                return character >= '0' && character <= '9';
            });
        }

        int ParseEmulatorPort(const std::string &serial) {
            constexpr std::string_view PREFIX = "emulator-";
            if (!serial.starts_with(PREFIX)) {
                return 0;
            }
            int port = 0;
            const std::string_view value(serial.data() + PREFIX.size(), serial.size() - PREFIX.size());
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
            return error == std::errc{} && end == value.data() + value.size() ? port : 0;
        }
    }

    AdbDeviceState ParseAdbDeviceState(const std::string &state) {
        if (state == "device") return AdbDeviceState::Device;
        if (state == "offline") return AdbDeviceState::Offline;
        if (state == "unauthorized") return AdbDeviceState::Unauthorized;
        if (state == "recovery") return AdbDeviceState::Recovery;
        if (state == "bootloader") return AdbDeviceState::Bootloader;
        if (state == "sideload") return AdbDeviceState::Sideload;
        if (state == "no permissions") return AdbDeviceState::NoPermissions;
        return AdbDeviceState::Unknown;
    }

    std::vector<AdbDevice> ParseAdbDevices(const std::string &output) {
        std::vector<AdbDevice> devices;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            line = Platform::Trim(line);
            if (line.empty() || line == "List of devices attached" || line.starts_with("* daemon")) {
                continue;
            }

            std::istringstream fields(line);
            AdbDevice device;
            if (!(fields >> device.Serial)) {
                continue;
            }

            std::string state;
            if (!(fields >> state)) {
                continue;
            }
            if (state == "no" && line.find("no permissions") != std::string::npos) {
                state = "no permissions";
            }
            device.StateText = state;
            device.State = ParseAdbDeviceState(state);

            std::string token;
            while (fields >> token) {
                const auto separator = token.find(':');
                if (separator == std::string::npos) {
                    continue;
                }
                const std::string key = token.substr(0, separator);
                const std::string value = token.substr(separator + 1);
                if (key == "product") {
                    device.Product = value;
                } else if (key == "model") {
                    device.Model = value;
                } else if (key == "device") {
                    device.Device = value;
                } else if (key == "transport_id") {
                    device.TransportId = value;
                }
            }

            device.EmulatorConsolePort = ParseEmulatorPort(device.Serial);
            device.IsEmulator = device.EmulatorConsolePort > 0;
            devices.push_back(std::move(device));
        }
        return devices;
    }

    std::vector<AdbForward> ParseAdbForwards(const std::string &output, const bool reverse) {
        std::vector<AdbForward> forwards;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            std::istringstream fields(line);
            std::string first;
            std::string second;
            std::string third;
            if (!(fields >> first >> second)) {
                continue;
            }
            const bool hasSerial = static_cast<bool>(fields >> third);
            AdbForward forward;
            forward.Reverse = reverse;
            if (hasSerial) {
                forward.Serial = std::move(first);
                forward.Local = std::move(second);
                forward.Remote = std::move(third);
            } else if (reverse) {
                forward.Local = std::move(first);
                forward.Remote = std::move(second);
            } else {
                continue;
            }

            if (reverse && IsReverseHostId(forward.Serial)) {
                forward.Serial.clear();
            }
            forwards.push_back(std::move(forward));
        }
        return forwards;
    }
}

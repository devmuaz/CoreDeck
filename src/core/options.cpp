//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <fstream>
#include <rfl/json.hpp>

#include "options.h"
#include "log.h"
#include "paths.h"

namespace CoreDeck {
    namespace {
        const char *FindDisplayLabel(const std::vector<OptionValueLabel> &options, const std::string &value) {
            for (const auto &[Label, RawValue]: options) {
                if (value == RawValue) {
                    return Label;
                }
            }
            return value.c_str();
        }
    }

    const std::vector<OptionValueLabel> &GpuModeOptions() {
        static const std::vector<OptionValueLabel> OPTIONS = {
            {.Label = "Automatic", .Value = "auto"},
            {.Label = "Hardware Acceleration", .Value = "host"},
            {.Label = "Software Rendering", .Value = "swiftshader_indirect"},
            {.Label = "ANGLE Rendering", .Value = "angle_indirect"},
            {.Label = "Guest Rendering", .Value = "guest"},
        };
        return OPTIONS;
    }

    const char *GpuModeDisplayLabel(const std::string &value) {
        return FindDisplayLabel(GpuModeOptions(), value);
    }

    const char *ScreenModeDisplayLabel(const std::string &value) {
        static const std::vector<OptionValueLabel> OPTIONS = {
            {.Label = "Touch Screen", .Value = "touch"},
            {.Label = "Multi-Touch Screen", .Value = "multi-touch"},
            {.Label = "No Touch Input", .Value = "no-touch"},
        };
        return FindDisplayLabel(OPTIONS, value);
    }

    namespace {
        const char *NetworkSpeedDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = "Full Speed", .Value = "full"},
                {.Label = "LTE", .Value = "lte"},
                {.Label = "HSDPA", .Value = "hsdpa"},
                {.Label = "UMTS", .Value = "umts"},
                {.Label = "EDGE", .Value = "edge"},
                {.Label = "GPRS", .Value = "gprs"},
                {.Label = "GSM", .Value = "gsm"},
            };
            return FindDisplayLabel(OPTIONS, value);
        }

        const char *NetworkDelayDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = "No Delay", .Value = "none"},
                {.Label = "GPRS Latency", .Value = "gprs"},
                {.Label = "EDGE Latency", .Value = "edge"},
                {.Label = "UMTS Latency", .Value = "umts"},
            };
            return FindDisplayLabel(OPTIONS, value);
        }

        const char *AccelerationModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = "Automatic", .Value = "auto"},
                {.Label = "Disabled", .Value = "off"},
                {.Label = "Enabled", .Value = "on"},
            };
            return FindDisplayLabel(OPTIONS, value);
        }

        const char *SELinuxModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = "Permissive", .Value = "permissive"},
                {.Label = "Disabled", .Value = "disabled"},
            };
            return FindDisplayLabel(OPTIONS, value);
        }

        const char *CameraModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = "Virtual Scene", .Value = "virtualscene"},
                {.Label = "Emulated", .Value = "emulated"},
                {.Label = "None", .Value = "none"},
            };
            return FindDisplayLabel(OPTIONS, value);
        }
    }

    const char *EmulatorOptionItemDisplayLabel(const std::string &flag, const std::string &value) {
        if (flag == "-gpu") {
            return GpuModeDisplayLabel(value);
        }
        if (flag == "-screen") {
            return ScreenModeDisplayLabel(value);
        }
        if (flag == "-netspeed") {
            return NetworkSpeedDisplayLabel(value);
        }
        if (flag == "-netdelay") {
            return NetworkDelayDisplayLabel(value);
        }
        if (flag == "-accel") {
            return AccelerationModeDisplayLabel(value);
        }
        if (flag == "-selinux") {
            return SELinuxModeDisplayLabel(value);
        }
        if (flag == "-camera-back" || flag == "-camera-front") {
            return CameraModeDisplayLabel(value);
        }
        return value.c_str();
    }

    namespace {
        std::vector<EmulatorOption> DisplayOptions() {
            return {
                {
                    .Flag = "-gpu",
                    .DisplayName = "GPU Mode",
                    .Description = "Set hardware OpenGLES emulation mode",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"auto", "host", "swiftshader_indirect", "angle_indirect", "guest"},
                    .SelectedItem = 0,
                },
                {
                    .Flag = "-screen",
                    .DisplayName = "Screen Mode",
                    .Description = "Set emulated screen mode",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"touch", "multi-touch", "no-touch"},
                },
                {
                    .Flag = "-dpi-device",
                    .DisplayName = "Device DPI",
                    .Description = "Override the device's screen density in dpi",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = "e.g., 420",
                },
                {
                    .Flag = "-skin",
                    .DisplayName = "Skin",
                    .Description = "Select a specific device skin by name",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = "e.g., pixel_7",
                },
                {
                    .Flag = "-no-skin",
                    .DisplayName = "Disable Skin",
                    .Description = "Run without any device skin",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::DISPLAY,
                },
                {
                    .Flag = "-window-size",
                    .DisplayName = "Window Size",
                    .Description = "Set the initial emulator window size (useful with no-skin)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = "e.g., 1080x1920",
                },
            };
        }

        std::vector<EmulatorOption> PerformanceOptions() {
            return {
                {
                    .Flag = "-memory",
                    .DisplayName = "Physical RAM (MBs)",
                    .Description = "Physical RAM size in MBs",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = "e.g., 2048",
                },
                {
                    .Flag = "-cores",
                    .DisplayName = "CPU Cores",
                    .Description = "Set number of CPU cores for the emulator",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = "e.g., 4",
                },
                {
                    .Flag = "-cache-size",
                    .DisplayName = "Cache Size (MBs)",
                    .Description = "Cache partition size in MBs",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = "e.g., 512",
                },
            };
        }

        std::vector<EmulatorOption> BootOptions() {
            return {
                {
                    .Flag = "-no-snapshot",
                    .DisplayName = "Full Boot",
                    .Description = "Perform a full boot and do not auto-save on exit",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-load",
                    .DisplayName = "Cold Boot",
                    .Description = "Perform a full boot without loading a snapshot (preserves user data)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-save",
                    .DisplayName = "Discard State on Exit",
                    .Description = "Do not auto-save to snapshot on exit; changed state is abandoned",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-snapshot",
                    .DisplayName = "Snapshot Name",
                    .Description = "Name of the snapshot to auto-start from and auto-save to",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::BOOT,
                    .Hint = "e.g., default-boot",
                },
                {
                    .Flag = "-read-only",
                    .DisplayName = "Read-Only (Multi-Instance)",
                    .Description = "Allow running multiple instances of this AVD (snapshots cannot be saved)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-wipe-data",
                    .DisplayName = "Factory Reset",
                    .Description = "Reset AVD to factory defaults (clears user data)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-boot-anim",
                    .DisplayName = "Skip Boot Animation",
                    .Description = "Disable boot animation for faster startup",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
            };
        }

        std::vector<EmulatorOption> AudioOptions() {
            return {
                {
                    .Flag = "-no-audio",
                    .DisplayName = "Disable Audio",
                    .Description = "Disable audio support",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
                {
                    .Flag = "-allow-host-audio",
                    .DisplayName = "Allow Host Microphone",
                    .Description = "Pass host audio input devices through to the guest (otherwise zeroed)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
            };
        }

        std::vector<EmulatorOption> NetworkOptions() {
            return {
                {
                    .Flag = "-netspeed",
                    .DisplayName = "Network Speed",
                    .Description = "Simulate network download/upload speed",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"full", "lte", "hsdpa", "umts", "edge", "gprs", "gsm"},
                },
                {
                    .Flag = "-netdelay",
                    .DisplayName = "Network Delay",
                    .Description = "Simulate network latency",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"none", "gprs", "edge", "umts"},
                },
                {
                    .Flag = "-http-proxy",
                    .DisplayName = "HTTP Proxy",
                    .Description = "Route network traffic through a HTTP/HTTPS proxy (e.g., Charles, mitmproxy)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = "e.g., http://localhost:8888",
                },
                {
                    .Flag = "-dns-server",
                    .DisplayName = "DNS Server",
                    .Description = "Use custom DNS server(s) in the emulated system",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = "e.g., 8.8.8.8",
                },
                {
                    .Flag = "-tcpdump",
                    .DisplayName = "Packet Capture File",
                    .Description = "Capture network packets to the given pcap file",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = "e.g., /tmp/emulator.pcap",
                },
                {
                    .Flag = "-port",
                    .DisplayName = "Console Port",
                    .Description = "TCP port used for the emulator console (adb port is console + 1)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = "e.g., 5554",
                },
                {
                    .Flag = "-ports",
                    .DisplayName = "Console + ADB Ports",
                    .Description = "TCP ports used for the console and adb bridge",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = "e.g., 5554,5555",
                },
            };
        }

        std::vector<EmulatorOption> CameraOptions() {
            return {
                {
                    .Flag = "-camera-back",
                    .DisplayName = "Back Camera",
                    .Description = "Set emulation mode for the back-facing camera",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::CAMERA,
                    .Items = {"virtualscene", "emulated", "none"},
                },
                {
                    .Flag = "-camera-front",
                    .DisplayName = "Front Camera",
                    .Description = "Set emulation mode for the front-facing camera",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::CAMERA,
                    .Items = {"emulated", "none"},
                },
            };
        }

        std::vector<EmulatorOption> LocationOptions() {
            return {
                {
                    .Flag = "-no-passive-gps",
                    .DisplayName = "Disable Passive GPS",
                    .Description = "Disable passive GPS updates from the host",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::LOCATION,
                },
                {
                    .Flag = "-gnss-file-path",
                    .DisplayName = "GNSS Replay File",
                    .Description = "Read GNSS data from the given file to replay a route",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::LOCATION,
                    .Hint = "e.g., /path/to/track.nmea",
                },
            };
        }

        std::vector<EmulatorOption> SystemOptions() {
            return {
                {
                    .Flag = "-phone-number",
                    .DisplayName = "Phone Number",
                    .Description = "Set the phone number reported by the emulated device",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = "e.g., +15555550100",
                },
                {
                    .Flag = "-change-locale",
                    .DisplayName = "Locale",
                    .Description = "Override the device locale (restarts the framework)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = "e.g., en-US",
                },
                {
                    .Flag = "-writable-system",
                    .DisplayName = "Writable System",
                    .Description = "Make the system and vendor images writable after 'adb remount'",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-skip-adb-auth",
                    .DisplayName = "Skip ADB Auth",
                    .Description = "Skip the adb authentication dialog on connect",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-id",
                    .DisplayName = "Instance ID",
                    .Description = "Assign a separate id to this virtual device (independent of the AVD name)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = "e.g., my-instance-1",
                },
                {
                    .Flag = "-prop",
                    .DisplayName = "System Property",
                    .Description = "Set a system property on boot (single name=value pair)",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = "e.g., ro.debuggable=1",
                },
                {
                    .Flag = "-feature",
                    .DisplayName = "Emulator Features",
                    .Description = "Force-enable or disable (-name) emulator features",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = "e.g., GLESDynamicVersion,-Vulkan",
                },
            };
        }

        std::vector<EmulatorOption> AdvancedOptions() {
            return {
                {
                    .Flag = "-no-window",
                    .DisplayName = "Headless Mode",
                    .Description = "Run without graphical window display (useful for CI/testing)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-show-kernel",
                    .DisplayName = "Show Kernel Log",
                    .Description = "Display kernel messages in the output log",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-verbose",
                    .DisplayName = "Verbose Logging",
                    .Description = "Enable verbose emulator logging (same as -debug-init)",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-wait-for-debugger",
                    .DisplayName = "Wait for Debugger",
                    .Description = "Pause on launch until a debugger process attaches",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-no-hidpi-scaling",
                    .DisplayName = "Disable HiDPI",
                    .Description = "Disable HiDPI scaling on macOS Retina displays",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-partition-size",
                    .DisplayName = "Partition Size (MBs)",
                    .Description = "System/data partition size in MBs",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., 2048",
                },
                {
                    .Flag = "-logcat",
                    .DisplayName = "Logcat Tags",
                    .Description = "Enable logcat output with specific tags",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., *:W or ActivityManager:I",
                },
                {
                    .Flag = "-timezone",
                    .DisplayName = "Timezone",
                    .Description = "Use a specific timezone instead of the host's default",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., America/New_York",
                },
                {
                    .Flag = "-accel",
                    .DisplayName = "Acceleration Mode",
                    .Description = "Configure emulation acceleration",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"auto", "off", "on"},
                },
                {
                    .Flag = "-selinux",
                    .DisplayName = "SELinux Mode",
                    .Description = "Set SELinux to disabled or permissive mode",
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"permissive", "disabled"},
                },
                {
                    .Flag = "-system",
                    .DisplayName = "System Image",
                    .Description = "Override the initial system image file",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., /path/to/system.img",
                },
                {
                    .Flag = "-data",
                    .DisplayName = "Userdata Image",
                    .Description = "Override the userdata image file",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., /path/to/userdata-qemu.img",
                },
                {
                    .Flag = "-sdcard",
                    .DisplayName = "SD Card Image",
                    .Description = "Override the SD card image file",
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = "e.g., /path/to/sdcard.img",
                },
                {
                    .Flag = "-restart-when-stalled",
                    .DisplayName = "Restart When Stalled",
                    .Description = "Automatically restart the guest if it becomes unresponsive",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-detect-image-hang",
                    .DisplayName = "Detect Image Hangs",
                    .Description = "Enable detection of system image hangs",
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
            };
        }
    }

    std::vector<EmulatorOption> GetEmulatorOptions() {
        std::vector<EmulatorOption> result;
        result.reserve(52);

        for (const auto &group: {
                 DisplayOptions(),
                 PerformanceOptions(),
                 BootOptions(),
                 AudioOptions(),
                 NetworkOptions(),
                 CameraOptions(),
                 LocationOptions(),
                 SystemOptions(),
                 AdvancedOptions(),
             }) {
            result.insert(result.end(), std::make_move_iterator(group.begin()), std::make_move_iterator(group.end()));
        }
        return result;
    }

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options) {
        std::vector<std::string> args;
        args.emplace_back("-avd");
        args.emplace_back(avdName);

        for (const auto &option: options) {
            if (!option.Enabled) {
                continue;
            }

            args.emplace_back(option.Flag);

            switch (option.Type) {
                case OptionType::TextInput:
                    if (!option.InputValue.empty()) {
                        args.emplace_back(option.InputValue);
                    }
                    break;

                case OptionType::Selection:
                    if (!option.Items.empty()) {
                        args.emplace_back(option.Items[option.SelectedItem]);
                    }
                    break;

                default:
                    // Otherwise Type would be ::Default (which is only Enabled or not)
                    break;
            }
        }

        return args;
    }

    std::string GetOptionsConfigPath(const std::string &avdName) {
        return Paths::GetOptionsConfigPath(avdName);
    }

    void EnsureOptionsConfigDirectoryExists() {
        Paths::EnsureOptionsConfigDirectoryExists();
    }

    void SaveOptionsToFile(const std::string &filePath, const std::vector<EmulatorOption> &options) {
        try {
            const auto json = rfl::json::write(options);
            std::ofstream file(filePath);
            if (!file.is_open()) {
                Log::Error("Failed to save options to: ", filePath);
                return;
            }
            file << json;
            file.close();
        } catch (const std::exception &e) {
            Log::Error("Failed to serialize options: ", e.what());
        }
    }

    std::vector<EmulatorOption> LoadOptionsFromFile(const std::string &filePath) {
        try {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                return GetEmulatorOptions();
            }

            const std::string json((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
            file.close();

            if (json.empty()) {
                return GetEmulatorOptions();
            }

            auto savedResult = rfl::json::read<std::vector<EmulatorOption>>(json);
            if (!savedResult) {
                return GetEmulatorOptions();
            }
            const auto &saved = savedResult.value();

            auto merged = GetEmulatorOptions();
            for (auto &option: merged) {
                const auto it = std::ranges::find_if(saved, [&](const EmulatorOption &s) {
                    return s.Flag == option.Flag;
                });
                if (it == saved.end()) {
                    continue;
                }
                option.Enabled = it->Enabled;
                option.InputValue = it->InputValue;
                if (option.Type == OptionType::Selection && !option.Items.empty()) {
                    const int clamped = std::clamp(it->SelectedItem, 0, static_cast<int>(option.Items.size()) - 1);
                    option.SelectedItem = clamped;
                }
            }
            return merged;
        } catch (const std::exception &e) {
            Log::Error("Failed to load options from ", filePath, ": ", e.what());
            return GetEmulatorOptions();
        }
    }
}

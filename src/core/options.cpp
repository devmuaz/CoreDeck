//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <fstream>
#include <rfl/json.hpp>

#include "options.h"
#include "i18n.h"
#include "log.h"
#include "paths.h"

namespace CoreDeck {
    namespace {
        const char *FindDisplayLabel(const std::vector<OptionValueLabel> &options, const std::string &value) {
            for (const auto &[label, rawValue]: options) {
                if (value == rawValue) {
                    return label;
                }
            }
            return value.c_str();
        }
    }

    const std::vector<OptionValueLabel> &GpuModeOptions() {
        static const std::vector<OptionValueLabel> OPTIONS = {
            {.Label = TrNoop("Automatic"), .Value = "auto"},
            {.Label = TrNoop("Hardware Acceleration"), .Value = "host"},
            {.Label = TrNoop("Software Rendering"), .Value = "swiftshader_indirect"},
            {.Label = TrNoop("ANGLE Rendering"), .Value = "angle_indirect"},
            {.Label = TrNoop("Guest Rendering"), .Value = "guest"},
        };
        return OPTIONS;
    }

    const char *GpuModeDisplayLabel(const std::string &value) {
        return Tr(FindDisplayLabel(GpuModeOptions(), value));
    }

    const char *ScreenModeDisplayLabel(const std::string &value) {
        static const std::vector<OptionValueLabel> OPTIONS = {
            {.Label = TrNoop("Touch Screen"), .Value = "touch"},
            {.Label = TrNoop("Multi-Touch Screen"), .Value = "multi-touch"},
            {.Label = TrNoop("No Touch Input"), .Value = "no-touch"},
        };
        return Tr(FindDisplayLabel(OPTIONS, value));
    }

    namespace {
        const char *NetworkSpeedDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = TrNoop("Full Speed"), .Value = "full"},
                {.Label = TrNoop("LTE"), .Value = "lte"},
                {.Label = TrNoop("HSDPA"), .Value = "hsdpa"},
                {.Label = TrNoop("UMTS"), .Value = "umts"},
                {.Label = TrNoop("EDGE"), .Value = "edge"},
                {.Label = TrNoop("GPRS"), .Value = "gprs"},
                {.Label = TrNoop("GSM"), .Value = "gsm"},
            };
            return Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *NetworkDelayDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = TrNoop("No Delay"), .Value = "none"},
                {.Label = TrNoop("GPRS Latency"), .Value = "gprs"},
                {.Label = TrNoop("EDGE Latency"), .Value = "edge"},
                {.Label = TrNoop("UMTS Latency"), .Value = "umts"},
            };
            return Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *AccelerationModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = TrNoop("Automatic"), .Value = "auto"},
                {.Label = TrNoop("Disabled"), .Value = "off"},
                {.Label = TrNoop("Enabled"), .Value = "on"},
            };
            return Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *SELinuxModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = TrNoop("Permissive"), .Value = "permissive"},
                {.Label = TrNoop("Disabled"), .Value = "disabled"},
            };
            return Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *CameraModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = TrNoop("Virtual Scene"), .Value = "virtualscene"},
                {.Label = TrNoop("Emulated"), .Value = "emulated"},
                {.Label = TrNoop("None"), .Value = "none"},
            };
            return Tr(FindDisplayLabel(OPTIONS, value));
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
                    .DisplayName = TrNoop("GPU Mode"),
                    .Description = TrNoop("Set hardware OpenGLES emulation mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"auto", "host", "swiftshader_indirect", "angle_indirect", "guest"},
                    .SelectedItem = 0,
                },
                {
                    .Flag = "-screen",
                    .DisplayName = TrNoop("Screen Mode"),
                    .Description = TrNoop("Set emulated screen mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"touch", "multi-touch", "no-touch"},
                },
                {
                    .Flag = "-dpi-device",
                    .DisplayName = TrNoop("Device DPI"),
                    .Description = TrNoop("Override the device's screen density in dpi"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = TrNoop("e.g., 420"),
                },
                {
                    .Flag = "-skin",
                    .DisplayName = TrNoop("Skin"),
                    .Description = TrNoop("Select a specific device skin by name"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = TrNoop("e.g., pixel_7"),
                },
                {
                    .Flag = "-no-skin",
                    .DisplayName = TrNoop("Disable Skin"),
                    .Description = TrNoop("Run without any device skin"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::DISPLAY,
                },
                {
                    .Flag = "-window-size",
                    .DisplayName = TrNoop("Window Size"),
                    .Description = TrNoop("Set the initial emulator window size (useful with no-skin)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = TrNoop("e.g., 1080x1920"),
                },
            };
        }

        std::vector<EmulatorOption> PerformanceOptions() {
            return {
                {
                    .Flag = "-memory",
                    .DisplayName = TrNoop("Physical RAM (MBs)"),
                    .Description = TrNoop("Physical RAM size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = TrNoop("e.g., 2048"),
                },
                {
                    .Flag = "-cores",
                    .DisplayName = TrNoop("CPU Cores"),
                    .Description = TrNoop("Set number of CPU cores for the emulator"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = TrNoop("e.g., 4"),
                },
                {
                    .Flag = "-cache-size",
                    .DisplayName = TrNoop("Cache Size (MBs)"),
                    .Description = TrNoop("Cache partition size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = TrNoop("e.g., 512"),
                },
            };
        }

        std::vector<EmulatorOption> BootOptions() {
            return {
                {
                    .Flag = "-no-snapshot",
                    .DisplayName = TrNoop("Full Boot"),
                    .Description = TrNoop("Perform a full boot and do not auto-save on exit"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-load",
                    .DisplayName = TrNoop("Cold Boot"),
                    .Description = TrNoop("Perform a full boot without loading a snapshot (preserves user data)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-save",
                    .DisplayName = TrNoop("Discard State on Exit"),
                    .Description = TrNoop("Do not auto-save to snapshot on exit; changed state is abandoned"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-snapshot",
                    .DisplayName = TrNoop("Snapshot Name"),
                    .Description = TrNoop("Name of the snapshot to auto-start from and auto-save to"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::BOOT,
                    .Hint = TrNoop("e.g., default-boot"),
                },
                {
                    .Flag = "-read-only",
                    .DisplayName = TrNoop("Read-Only (Multi-Instance)"),
                    .Description = TrNoop("Allow running multiple instances of this AVD (snapshots cannot be saved)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-wipe-data",
                    .DisplayName = TrNoop("Factory Reset"),
                    .Description = TrNoop("Reset AVD to factory defaults (clears user data)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-boot-anim",
                    .DisplayName = TrNoop("Skip Boot Animation"),
                    .Description = TrNoop("Disable boot animation for faster startup"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
            };
        }

        std::vector<EmulatorOption> AudioOptions() {
            return {
                {
                    .Flag = "-no-audio",
                    .DisplayName = TrNoop("Disable Audio"),
                    .Description = TrNoop("Disable audio support"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
                {
                    .Flag = "-allow-host-audio",
                    .DisplayName = TrNoop("Allow Host Microphone"),
                    .Description = TrNoop("Pass host audio input devices through to the guest (otherwise zeroed)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
            };
        }

        std::vector<EmulatorOption> NetworkOptions() {
            return {
                {
                    .Flag = "-netspeed",
                    .DisplayName = TrNoop("Network Speed"),
                    .Description = TrNoop("Simulate network download/upload speed"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"full", "lte", "hsdpa", "umts", "edge", "gprs", "gsm"},
                },
                {
                    .Flag = "-netdelay",
                    .DisplayName = TrNoop("Network Delay"),
                    .Description = TrNoop("Simulate network latency"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"none", "gprs", "edge", "umts"},
                },
                {
                    .Flag = "-http-proxy",
                    .DisplayName = TrNoop("HTTP Proxy"),
                    .Description = TrNoop("Route network traffic through a HTTP/HTTPS proxy (e.g., Charles, mitmproxy)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = TrNoop("e.g., http://localhost:8888"),
                },
                {
                    .Flag = "-dns-server",
                    .DisplayName = TrNoop("DNS Server"),
                    .Description = TrNoop("Use custom DNS server(s) in the emulated system"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = TrNoop("e.g., 8.8.8.8"),
                },
                {
                    .Flag = "-tcpdump",
                    .DisplayName = TrNoop("Packet Capture File"),
                    .Description = TrNoop("Capture network packets to the given pcap file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = TrNoop("e.g., /tmp/emulator.pcap"),
                },
                {
                    .Flag = "-port",
                    .DisplayName = TrNoop("Console Port"),
                    .Description = TrNoop("TCP port used for the emulator console (adb port is console + 1)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = TrNoop("e.g., 5554"),
                },
                {
                    .Flag = "-ports",
                    .DisplayName = TrNoop("Console + ADB Ports"),
                    .Description = TrNoop("TCP ports used for the console and adb bridge"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = TrNoop("e.g., 5554,5555"),
                },
            };
        }

        std::vector<EmulatorOption> CameraOptions() {
            return {
                {
                    .Flag = "-camera-back",
                    .DisplayName = TrNoop("Back Camera"),
                    .Description = TrNoop("Set emulation mode for the back-facing camera"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::CAMERA,
                    .Items = {"virtualscene", "emulated", "none"},
                },
                {
                    .Flag = "-camera-front",
                    .DisplayName = TrNoop("Front Camera"),
                    .Description = TrNoop("Set emulation mode for the front-facing camera"),
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
                    .DisplayName = TrNoop("Disable Passive GPS"),
                    .Description = TrNoop("Disable passive GPS updates from the host"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::LOCATION,
                },
                {
                    .Flag = "-gnss-file-path",
                    .DisplayName = TrNoop("GNSS Replay File"),
                    .Description = TrNoop("Read GNSS data from the given file to replay a route"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::LOCATION,
                    .Hint = TrNoop("e.g., /path/to/track.nmea"),
                },
            };
        }

        std::vector<EmulatorOption> SystemOptions() {
            return {
                {
                    .Flag = "-phone-number",
                    .DisplayName = TrNoop("Phone Number"),
                    .Description = TrNoop("Set the phone number reported by the emulated device"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = TrNoop("e.g., +15555550100"),
                },
                {
                    .Flag = "-change-locale",
                    .DisplayName = TrNoop("Locale"),
                    .Description = TrNoop("Override the device locale (restarts the framework)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = TrNoop("e.g., en-US"),
                },
                {
                    .Flag = "-writable-system",
                    .DisplayName = TrNoop("Writable System"),
                    .Description = TrNoop("Make the system and vendor images writable after 'adb remount'"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-skip-adb-auth",
                    .DisplayName = TrNoop("Skip ADB Auth"),
                    .Description = TrNoop("Skip the adb authentication dialog on connect"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-id",
                    .DisplayName = TrNoop("Instance ID"),
                    .Description = TrNoop("Assign a separate id to this virtual device (independent of the AVD name)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = TrNoop("e.g., my-instance-1"),
                },
                {
                    .Flag = "-prop",
                    .DisplayName = TrNoop("System Property"),
                    .Description = TrNoop("Set a system property on boot (single name=value pair)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = TrNoop("e.g., ro.debuggable=1"),
                },
                {
                    .Flag = "-feature",
                    .DisplayName = TrNoop("Emulator Features"),
                    .Description = TrNoop("Force-enable or disable (-name) emulator features"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = TrNoop("e.g., GLESDynamicVersion,-Vulkan"),
                },
            };
        }

        std::vector<EmulatorOption> AdvancedOptions() {
            return {
                {
                    .Flag = "-no-window",
                    .DisplayName = TrNoop("Headless Mode"),
                    .Description = TrNoop("Run without graphical window display (useful for CI/testing)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-show-kernel",
                    .DisplayName = TrNoop("Show Kernel Log"),
                    .Description = TrNoop("Display kernel messages in the output log"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-verbose",
                    .DisplayName = TrNoop("Verbose Logging"),
                    .Description = TrNoop("Enable verbose emulator logging (same as -debug-init)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-wait-for-debugger",
                    .DisplayName = TrNoop("Wait for Debugger"),
                    .Description = TrNoop("Pause on launch until a debugger process attaches"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-no-hidpi-scaling",
                    .DisplayName = TrNoop("Disable HiDPI"),
                    .Description = TrNoop("Disable HiDPI scaling on macOS Retina displays"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-partition-size",
                    .DisplayName = TrNoop("Partition Size (MBs)"),
                    .Description = TrNoop("System/data partition size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., 2048"),
                },
                {
                    .Flag = "-logcat",
                    .DisplayName = TrNoop("Logcat Tags"),
                    .Description = TrNoop("Enable logcat output with specific tags"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., *:W or ActivityManager:I"),
                },
                {
                    .Flag = "-timezone",
                    .DisplayName = TrNoop("Timezone"),
                    .Description = TrNoop("Use a specific timezone instead of the host's default"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., America/New_York"),
                },
                {
                    .Flag = "-accel",
                    .DisplayName = TrNoop("Acceleration Mode"),
                    .Description = TrNoop("Configure emulation acceleration"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"auto", "off", "on"},
                },
                {
                    .Flag = "-selinux",
                    .DisplayName = TrNoop("SELinux Mode"),
                    .Description = TrNoop("Set SELinux to disabled or permissive mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"permissive", "disabled"},
                },
                {
                    .Flag = "-system",
                    .DisplayName = TrNoop("System Image"),
                    .Description = TrNoop("Override the initial system image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., /path/to/system.img"),
                },
                {
                    .Flag = "-data",
                    .DisplayName = TrNoop("Userdata Image"),
                    .Description = TrNoop("Override the userdata image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., /path/to/userdata-qemu.img"),
                },
                {
                    .Flag = "-sdcard",
                    .DisplayName = TrNoop("SD Card Image"),
                    .Description = TrNoop("Override the SD card image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = TrNoop("e.g., /path/to/sdcard.img"),
                },
                {
                    .Flag = "-restart-when-stalled",
                    .DisplayName = TrNoop("Restart When Stalled"),
                    .Description = TrNoop("Automatically restart the guest if it becomes unresponsive"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-detect-image-hang",
                    .DisplayName = TrNoop("Detect Image Hangs"),
                    .Description = TrNoop("Enable detection of system image hangs"),
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
                        args.emplace_back(option.Items.at(option.SelectedItem));
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

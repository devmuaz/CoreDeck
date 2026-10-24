//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <fstream>
#include <rfl/json.hpp>

#include "options.h"
#include "platform/i18n.h"
#include "platform/log.h"

namespace CoreDeck::Core {
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
            {.Label = Platform::TrNoop("Automatic"), .Value = "auto"},
            {.Label = Platform::TrNoop("Hardware Acceleration"), .Value = "host"},
            {.Label = Platform::TrNoop("Software Rendering"), .Value = "swiftshader_indirect"},
            {.Label = Platform::TrNoop("ANGLE Rendering"), .Value = "angle_indirect"},
            {.Label = Platform::TrNoop("Guest Rendering"), .Value = "guest"},
        };
        return OPTIONS;
    }

    const char *GpuModeDisplayLabel(const std::string &value) {
        return Platform::Tr(FindDisplayLabel(GpuModeOptions(), value));
    }

    const char *ScreenModeDisplayLabel(const std::string &value) {
        static const std::vector<OptionValueLabel> OPTIONS = {
            {.Label = Platform::TrNoop("Touch Screen"), .Value = "touch"},
            {.Label = Platform::TrNoop("Multi-Touch Screen"), .Value = "multi-touch"},
            {.Label = Platform::TrNoop("No Touch Input"), .Value = "no-touch"},
        };
        return Platform::Tr(FindDisplayLabel(OPTIONS, value));
    }

    namespace {
        const char *NetworkSpeedDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = Platform::TrNoop("Full Speed"), .Value = "full"},
                {.Label = Platform::TrNoop("LTE"), .Value = "lte"},
                {.Label = Platform::TrNoop("HSDPA"), .Value = "hsdpa"},
                {.Label = Platform::TrNoop("UMTS"), .Value = "umts"},
                {.Label = Platform::TrNoop("EDGE"), .Value = "edge"},
                {.Label = Platform::TrNoop("GPRS"), .Value = "gprs"},
                {.Label = Platform::TrNoop("GSM"), .Value = "gsm"},
            };
            return Platform::Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *NetworkDelayDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = Platform::TrNoop("No Delay"), .Value = "none"},
                {.Label = Platform::TrNoop("GPRS Latency"), .Value = "gprs"},
                {.Label = Platform::TrNoop("EDGE Latency"), .Value = "edge"},
                {.Label = Platform::TrNoop("UMTS Latency"), .Value = "umts"},
            };
            return Platform::Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *AccelerationModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = Platform::TrNoop("Automatic"), .Value = "auto"},
                {.Label = Platform::TrNoop("Disabled"), .Value = "off"},
                {.Label = Platform::TrNoop("Enabled"), .Value = "on"},
            };
            return Platform::Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *SELinuxModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = Platform::TrNoop("Permissive"), .Value = "permissive"},
                {.Label = Platform::TrNoop("Disabled"), .Value = "disabled"},
            };
            return Platform::Tr(FindDisplayLabel(OPTIONS, value));
        }

        const char *CameraModeDisplayLabel(const std::string &value) {
            static const std::vector<OptionValueLabel> OPTIONS = {
                {.Label = Platform::TrNoop("Virtual Scene"), .Value = "virtualscene"},
                {.Label = Platform::TrNoop("Emulated"), .Value = "emulated"},
                {.Label = Platform::TrNoop("None"), .Value = "none"},
            };
            return Platform::Tr(FindDisplayLabel(OPTIONS, value));
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
                    .DisplayName = Platform::TrNoop("GPU Mode"),
                    .Description = Platform::TrNoop("Set hardware OpenGLES emulation mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"auto", "host", "swiftshader_indirect", "angle_indirect", "guest"},
                    .SelectedItem = 0,
                },
                {
                    .Flag = "-screen",
                    .DisplayName = Platform::TrNoop("Screen Mode"),
                    .Description = Platform::TrNoop("Set emulated screen mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::DISPLAY,
                    .Items = {"touch", "multi-touch", "no-touch"},
                },
                {
                    .Flag = "-dpi-device",
                    .DisplayName = Platform::TrNoop("Device DPI"),
                    .Description = Platform::TrNoop("Override the device's screen density in dpi"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = Platform::TrNoop("e.g., 420"),
                },
                {
                    .Flag = "-skin",
                    .DisplayName = Platform::TrNoop("Skin"),
                    .Description = Platform::TrNoop("Select a specific device skin by name"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = Platform::TrNoop("e.g., pixel_7"),
                },
                {
                    .Flag = "-no-skin",
                    .DisplayName = Platform::TrNoop("Disable Skin"),
                    .Description = Platform::TrNoop("Run without any device skin"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::DISPLAY,
                },
                {
                    .Flag = "-window-size",
                    .DisplayName = Platform::TrNoop("Window Size"),
                    .Description = Platform::TrNoop("Set the initial emulator window size (useful with no-skin)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::DISPLAY,
                    .Hint = Platform::TrNoop("e.g., 1080x1920"),
                },
            };
        }

        std::vector<EmulatorOption> PerformanceOptions() {
            return {
                {
                    .Flag = "-memory",
                    .DisplayName = Platform::TrNoop("Physical RAM (MBs)"),
                    .Description = Platform::TrNoop("Physical RAM size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = Platform::TrNoop("e.g., 2048"),
                },
                {
                    .Flag = "-cores",
                    .DisplayName = Platform::TrNoop("CPU Cores"),
                    .Description = Platform::TrNoop("Set number of CPU cores for the emulator"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = Platform::TrNoop("e.g., 4"),
                },
                {
                    .Flag = "-cache-size",
                    .DisplayName = Platform::TrNoop("Cache Size (MBs)"),
                    .Description = Platform::TrNoop("Cache partition size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::PERFORMANCE,
                    .Hint = Platform::TrNoop("e.g., 512"),
                },
            };
        }

        std::vector<EmulatorOption> BootOptions() {
            return {
                {
                    .Flag = "-no-snapshot",
                    .DisplayName = Platform::TrNoop("Full Boot"),
                    .Description = Platform::TrNoop("Perform a full boot and do not auto-save on exit"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-load",
                    .DisplayName = Platform::TrNoop("Cold Boot"),
                    .Description = Platform::TrNoop("Perform a full boot without loading a snapshot (preserves user data)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-snapshot-save",
                    .DisplayName = Platform::TrNoop("Discard State on Exit"),
                    .Description = Platform::TrNoop("Do not auto-save to snapshot on exit; changed state is abandoned"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-snapshot",
                    .DisplayName = Platform::TrNoop("Snapshot Name"),
                    .Description = Platform::TrNoop("Name of the snapshot to auto-start from and auto-save to"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::BOOT,
                    .Hint = Platform::TrNoop("e.g., default-boot"),
                },
                {
                    .Flag = "-read-only",
                    .DisplayName = Platform::TrNoop("Read-Only (Multi-Instance)"),
                    .Description = Platform::TrNoop("Allow running multiple instances of this AVD (snapshots cannot be saved)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-wipe-data",
                    .DisplayName = Platform::TrNoop("Factory Reset"),
                    .Description = Platform::TrNoop("Reset AVD to factory defaults (clears user data)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
                {
                    .Flag = "-no-boot-anim",
                    .DisplayName = Platform::TrNoop("Skip Boot Animation"),
                    .Description = Platform::TrNoop("Disable boot animation for faster startup"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::BOOT,
                },
            };
        }

        std::vector<EmulatorOption> AudioOptions() {
            return {
                {
                    .Flag = "-no-audio",
                    .DisplayName = Platform::TrNoop("Disable Audio"),
                    .Description = Platform::TrNoop("Disable audio support"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
                {
                    .Flag = "-allow-host-audio",
                    .DisplayName = Platform::TrNoop("Allow Host Microphone"),
                    .Description = Platform::TrNoop("Pass host audio input devices through to the guest (otherwise zeroed)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::AUDIO,
                },
            };
        }

        std::vector<EmulatorOption> NetworkOptions() {
            return {
                {
                    .Flag = "-netspeed",
                    .DisplayName = Platform::TrNoop("Network Speed"),
                    .Description = Platform::TrNoop("Simulate network download/upload speed"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"full", "lte", "hsdpa", "umts", "edge", "gprs", "gsm"},
                },
                {
                    .Flag = "-netdelay",
                    .DisplayName = Platform::TrNoop("Network Delay"),
                    .Description = Platform::TrNoop("Simulate network latency"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::NETWORK,
                    .Items = {"none", "gprs", "edge", "umts"},
                },
                {
                    .Flag = "-http-proxy",
                    .DisplayName = Platform::TrNoop("HTTP Proxy"),
                    .Description = Platform::TrNoop("Route network traffic through a HTTP/HTTPS proxy (e.g., Charles, mitmproxy)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = Platform::TrNoop("e.g., http://localhost:8888"),
                },
                {
                    .Flag = "-dns-server",
                    .DisplayName = Platform::TrNoop("DNS Server"),
                    .Description = Platform::TrNoop("Use custom DNS server(s) in the emulated system"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = Platform::TrNoop("e.g., 8.8.8.8"),
                },
                {
                    .Flag = "-tcpdump",
                    .DisplayName = Platform::TrNoop("Packet Capture File"),
                    .Description = Platform::TrNoop("Capture network packets to the given pcap file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = Platform::TrNoop("e.g., /tmp/emulator.pcap"),
                },
                {
                    .Flag = "-port",
                    .DisplayName = Platform::TrNoop("Console Port"),
                    .Description = Platform::TrNoop("TCP port used for the emulator console (adb port is console + 1)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = Platform::TrNoop("e.g., 5554"),
                },
                {
                    .Flag = "-ports",
                    .DisplayName = Platform::TrNoop("Console + ADB Ports"),
                    .Description = Platform::TrNoop("TCP ports used for the console and adb bridge"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::NETWORK,
                    .Hint = Platform::TrNoop("e.g., 5554,5555"),
                },
            };
        }

        std::vector<EmulatorOption> CameraOptions() {
            return {
                {
                    .Flag = "-camera-back",
                    .DisplayName = Platform::TrNoop("Back Camera"),
                    .Description = Platform::TrNoop("Set emulation mode for the back-facing camera"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::CAMERA,
                    .Items = {"virtualscene", "emulated", "none"},
                },
                {
                    .Flag = "-camera-front",
                    .DisplayName = Platform::TrNoop("Front Camera"),
                    .Description = Platform::TrNoop("Set emulation mode for the front-facing camera"),
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
                    .DisplayName = Platform::TrNoop("Disable Passive GPS"),
                    .Description = Platform::TrNoop("Disable passive GPS updates from the host"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::LOCATION,
                },
                {
                    .Flag = "-gnss-file-path",
                    .DisplayName = Platform::TrNoop("GNSS Replay File"),
                    .Description = Platform::TrNoop("Read GNSS data from the given file to replay a route"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::LOCATION,
                    .Hint = Platform::TrNoop("e.g., /path/to/track.nmea"),
                },
            };
        }

        std::vector<EmulatorOption> SystemOptions() {
            return {
                {
                    .Flag = "-phone-number",
                    .DisplayName = Platform::TrNoop("Phone Number"),
                    .Description = Platform::TrNoop("Set the phone number reported by the emulated device"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = Platform::TrNoop("e.g., +15555550100"),
                },
                {
                    .Flag = "-change-locale",
                    .DisplayName = Platform::TrNoop("Locale"),
                    .Description = Platform::TrNoop("Override the device locale (restarts the framework)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = Platform::TrNoop("e.g., en-US"),
                },
                {
                    .Flag = "-writable-system",
                    .DisplayName = Platform::TrNoop("Writable System"),
                    .Description = Platform::TrNoop("Make the system and vendor images writable after 'adb remount'"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-skip-adb-auth",
                    .DisplayName = Platform::TrNoop("Skip ADB Auth"),
                    .Description = Platform::TrNoop("Skip the adb authentication dialog on connect"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::SYSTEM,
                },
                {
                    .Flag = "-id",
                    .DisplayName = Platform::TrNoop("Instance ID"),
                    .Description = Platform::TrNoop("Assign a separate id to this virtual device (independent of the AVD name)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = Platform::TrNoop("e.g., my-instance-1"),
                },
                {
                    .Flag = "-prop",
                    .DisplayName = Platform::TrNoop("System Property"),
                    .Description = Platform::TrNoop("Set a system property on boot (single name=value pair)"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = Platform::TrNoop("e.g., ro.debuggable=1"),
                },
                {
                    .Flag = "-feature",
                    .DisplayName = Platform::TrNoop("Emulator Features"),
                    .Description = Platform::TrNoop("Force-enable or disable (-name) emulator features"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::SYSTEM,
                    .Hint = Platform::TrNoop("e.g., GLESDynamicVersion,-Vulkan"),
                },
            };
        }

        std::vector<EmulatorOption> AdvancedOptions() {
            return {
                {
                    .Flag = "-no-window",
                    .DisplayName = Platform::TrNoop("Headless Mode"),
                    .Description = Platform::TrNoop("Run without graphical window display (useful for CI/testing)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-show-kernel",
                    .DisplayName = Platform::TrNoop("Show Kernel Log"),
                    .Description = Platform::TrNoop("Display kernel messages in the output log"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-verbose",
                    .DisplayName = Platform::TrNoop("Verbose Logging"),
                    .Description = Platform::TrNoop("Enable verbose emulator logging (same as -debug-init)"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-wait-for-debugger",
                    .DisplayName = Platform::TrNoop("Wait for Debugger"),
                    .Description = Platform::TrNoop("Pause on launch until a debugger process attaches"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-no-hidpi-scaling",
                    .DisplayName = Platform::TrNoop("Disable HiDPI"),
                    .Description = Platform::TrNoop("Disable HiDPI scaling on macOS Retina displays"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-partition-size",
                    .DisplayName = Platform::TrNoop("Partition Size (MBs)"),
                    .Description = Platform::TrNoop("System/data partition size in MBs"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., 2048"),
                },
                {
                    .Flag = "-logcat",
                    .DisplayName = Platform::TrNoop("Logcat Tags"),
                    .Description = Platform::TrNoop("Enable logcat output with specific tags"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., *:W or ActivityManager:I"),
                },
                {
                    .Flag = "-timezone",
                    .DisplayName = Platform::TrNoop("Timezone"),
                    .Description = Platform::TrNoop("Use a specific timezone instead of the host's default"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., America/New_York"),
                },
                {
                    .Flag = "-accel",
                    .DisplayName = Platform::TrNoop("Acceleration Mode"),
                    .Description = Platform::TrNoop("Configure emulation acceleration"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"auto", "off", "on"},
                },
                {
                    .Flag = "-selinux",
                    .DisplayName = Platform::TrNoop("SELinux Mode"),
                    .Description = Platform::TrNoop("Set SELinux to disabled or permissive mode"),
                    .Type = OptionType::Selection,
                    .Category = OptionCategory::ADVANCED,
                    .Items = {"permissive", "disabled"},
                },
                {
                    .Flag = "-system",
                    .DisplayName = Platform::TrNoop("System Image"),
                    .Description = Platform::TrNoop("Override the initial system image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., /path/to/system.img"),
                },
                {
                    .Flag = "-data",
                    .DisplayName = Platform::TrNoop("Userdata Image"),
                    .Description = Platform::TrNoop("Override the userdata image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., /path/to/userdata-qemu.img"),
                },
                {
                    .Flag = "-sdcard",
                    .DisplayName = Platform::TrNoop("SD Card Image"),
                    .Description = Platform::TrNoop("Override the SD card image file"),
                    .Type = OptionType::TextInput,
                    .Category = OptionCategory::ADVANCED,
                    .Hint = Platform::TrNoop("e.g., /path/to/sdcard.img"),
                },
                {
                    .Flag = "-restart-when-stalled",
                    .DisplayName = Platform::TrNoop("Restart When Stalled"),
                    .Description = Platform::TrNoop("Automatically restart the guest if it becomes unresponsive"),
                    .Type = OptionType::Default,
                    .Category = OptionCategory::ADVANCED,
                },
                {
                    .Flag = "-detect-image-hang",
                    .DisplayName = Platform::TrNoop("Detect Image Hangs"),
                    .Description = Platform::TrNoop("Enable detection of system image hangs"),
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


    void SaveOptionsToFile(const std::string &filePath, const std::vector<EmulatorOption> &options) {
        try {
            const auto json = rfl::json::write(options);
            std::ofstream file(filePath);
            if (!file.is_open()) {
                Platform::Log::Error("Failed to save options to: ", filePath);
                return;
            }
            file << json;
            file.close();
        } catch (const std::exception &e) {
            Platform::Log::Error("Failed to serialize options: ", e.what());
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
            Platform::Log::Error("Failed to load options from ", filePath, ": ", e.what());
            return GetEmulatorOptions();
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

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
            };
        }
    }

    std::vector<EmulatorOption> GetEmulatorOptions() {
        std::vector<EmulatorOption> result;
        result.reserve(22);

        for (const auto &group: {
                 DisplayOptions(),
                 PerformanceOptions(),
                 BootOptions(),
                 AudioOptions(),
                 NetworkOptions(),
                 CameraOptions(),
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

            auto options = rfl::json::read<std::vector<EmulatorOption>>(json);
            return options.value();
        } catch (const std::exception &e) {
            Log::Error("Failed to load options from ", filePath, ": ", e.what());
            return GetEmulatorOptions();
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "options.h"
#include "paths.h"
#include <fstream>
#include <iostream>
#include <rfl/json.hpp>

namespace CoreDeck {
    std::vector<EmulatorOption> GetEmulatorOptions() {
        return {
            // Display
            {
                .Flag = "-gpu",
                .DisplayName = "GPU Mode",
                .Description = "Set hardware OpenGLES emulation mode",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Display,
                .Items = {"host", "swiftshader_indirect", "angle_indirect", "guest"},
                .SelectedItem = 0,
            },
            {
                .Flag = "-screen",
                .DisplayName = "Screen Mode",
                .Description = "Set emulated screen mode",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Display,
                .Items = {"touch", "multi-touch", "no-touch"},
            },

            // Performance
            {
                .Flag = "-memory",
                .DisplayName = "Physical RAM (MBs)",
                .Description = "Physical RAM size in MBs",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Performance,
                .Hint = "e.g., 2048",
            },
            {
                .Flag = "-cores",
                .DisplayName = "CPU Cores",
                .Description = "Set number of CPU cores for the emulator",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Performance,
                .Hint = "e.g., 4",
            },

            // Boot
            {
                .Flag = "-no-snapshot",
                .DisplayName = "Full Boot",
                .Description = "Perform a full boot and do not auto-save on exit",
                .Type = OptionType::Default,
                .Category = OptionCategory::Boot,
            },
            {
                .Flag = "-wipe-data",
                .DisplayName = "Factory Reset",
                .Description = "Reset AVD to factory defaults (clears user data)",
                .Type = OptionType::Default,
                .Category = OptionCategory::Boot,
            },
            {
                .Flag = "-no-boot-anim",
                .DisplayName = "Skip Boot Animation",
                .Description = "Disable boot animation for faster startup",
                .Type = OptionType::Default,
                .Category = OptionCategory::Boot,
            },

            // Audio
            {
                .Flag = "-no-audio",
                .DisplayName = "Disable Audio",
                .Description = "Disable audio support",
                .Type = OptionType::Default,
                .Category = OptionCategory::Audio,
            },

            // Network
            {
                .Flag = "-netspeed",
                .DisplayName = "Network Speed",
                .Description = "Simulate network download/upload speed",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Network,
                .Items = {"full", "lte", "hsdpa", "umts", "edge", "gprs", "gsm"},
            },
            {
                .Flag = "-netdelay",
                .DisplayName = "Network Delay",
                .Description = "Simulate network latency",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Network,
                .Items = {"none", "gprs", "edge", "umts"},
            },
            {
                .Flag = "-http-proxy",
                .DisplayName = "HTTP Proxy",
                .Description = "Route network traffic through a HTTP/HTTPS proxy (e.g., Charles, mitmproxy)",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Network,
                .Hint = "e.g., http://localhost:8888",
            },
            {
                .Flag = "-dns-server",
                .DisplayName = "DNS Server",
                .Description = "Use custom DNS server(s) in the emulated system",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Network,
                .Hint = "e.g., 8.8.8.8",
            },

            // Camera
            {
                .Flag = "-camera-back",
                .DisplayName = "Back Camera",
                .Description = "Set emulation mode for the back-facing camera",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Camera,
                .Items = {"virtualscene", "emulated", "none"},
            },
            {
                .Flag = "-camera-front",
                .DisplayName = "Front Camera",
                .Description = "Set emulation mode for the front-facing camera",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Camera,
                .Items = {"emulated", "none"},
            },

            // Advanced
            {
                .Flag = "-no-window",
                .DisplayName = "Headless Mode",
                .Description = "Run without graphical window display (useful for CI/testing)",
                .Type = OptionType::Default,
                .Category = OptionCategory::Advanced,
            },
            {
                .Flag = "-show-kernel",
                .DisplayName = "Show Kernel Log",
                .Description = "Display kernel messages in the output log",
                .Type = OptionType::Default,
                .Category = OptionCategory::Advanced,
            },
            {
                .Flag = "-no-hidpi-scaling",
                .DisplayName = "Disable HiDPI",
                .Description = "Disable HiDPI scaling on macOS Retina displays",
                .Type = OptionType::Default,
                .Category = OptionCategory::Advanced,
            },
            {
                .Flag = "-partition-size",
                .DisplayName = "Partition Size (MBs)",
                .Description = "System/data partition size in MBs",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Advanced,
                .Hint = "e.g., 2048",
            },
            {
                .Flag = "-logcat",
                .DisplayName = "Logcat Tags",
                .Description = "Enable logcat output with specific tags",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Advanced,
                .Hint = "e.g., *:W or ActivityManager:I",
            },
            {
                .Flag = "-timezone",
                .DisplayName = "Timezone",
                .Description = "Use a specific timezone instead of the host's default",
                .Type = OptionType::TextInput,
                .Category = OptionCategory::Advanced,
                .Hint = "e.g., America/New_York",
            },
            {
                .Flag = "-accel",
                .DisplayName = "Acceleration Mode",
                .Description = "Configure emulation acceleration",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Advanced,
                .Items = {"auto", "off", "on"},
            },
            {
                .Flag = "-selinux",
                .DisplayName = "SELinux Mode",
                .Description = "Set SELinux to disabled or permissive mode",
                .Type = OptionType::Selection,
                .Category = OptionCategory::Advanced,
                .Items = {"permissive", "disabled"},
            },
        };
    }

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options) {
        std::vector<std::string> args;
        args.emplace_back("-avd");
        args.emplace_back(avdName);

        for (const auto &option: options) {
            if (!option.Enabled) continue;

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

    void EnsureConfigDirectoryExists() {
        Paths::EnsureConfigDirectoryExists();
    }

    void SaveOptionsToFile(const std::string &filePath, const std::vector<EmulatorOption> &options) {
        try {
            const auto json = rfl::json::write(options);
            std::ofstream file(filePath);
            if (!file.is_open()) {
                std::cerr << "Failed to save options to: " << filePath << std::endl;
                return;
            }
            file << json;
            file.close();
        } catch (const std::exception &e) {
            std::cerr << "Failed to serialize options: " << e.what() << std::endl;
        }
    }

    std::vector<EmulatorOption> LoadOptionsFromFile(const std::string &filePath) {
        try {
            std::ifstream file(filePath);
            if (!file.is_open()) return GetEmulatorOptions();

            const std::string json((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
            file.close();

            if (json.empty()) return GetEmulatorOptions();

            auto options = rfl::json::read<std::vector<EmulatorOption> >(json);
            return options.value();
        } catch (const std::exception &e) {
            std::cerr << "Failed to load options from " << filePath << ": " << e.what() << std::endl;
            return GetEmulatorOptions();
        }
    }
}

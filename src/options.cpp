//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "options.h"

namespace Emu {
    std::vector<EmulatorOption> GetEmulatorOptions() {
        return {
            {
                .Flag = "-gpu",
                .DisplayName = "Set GPU Mode",
                .Description = "Set GPU emulation mode (host, swiftshader_indirect, angle_indirect, guest)",
                .IsBoolean = false,
                .Enabled = false,
                .Hint = "e.g., host, swiftshader_indirect, ..."
            },
            {
                .Flag = "-memory",
                .DisplayName = "Set Physical RAM (MBs)",
                .Description = "Physical RAM size in MBs",
                .IsBoolean = false,
                .Enabled = false,
                .Hint = "e.g., 1024"
            },
            {
                .Flag = "-no-snapshot",
                .DisplayName = "Full Boot",
                .Description = "Perform a full boot and do not auto-save on exit",
                .IsBoolean = true
            },
            {
                .Flag = "-no-audio",
                .DisplayName = "Disable Audio",
                .Description = "Disable audio support",
                .IsBoolean = true
            },
            {
                .Flag = "-wipe-data",
                .DisplayName = "Factory Reset",
                .Description = "Reset AVD to factory defaults (clears user data)",
                .IsBoolean = true
            }
        };
    }

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options) {
        std::vector<std::string> args;
        args.emplace_back("-avd");
        args.emplace_back(avdName);

        for (const auto &option: options) {
            if (!option.Enabled) continue;

            args.emplace_back(option.Flag);

            if (!option.IsBoolean) {
                if (std::string val(option.ValueBuffer); !val.empty()) {
                    args.emplace_back(val);
                }
            }
        }

        return args;
    }
}

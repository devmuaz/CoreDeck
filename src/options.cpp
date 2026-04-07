//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "options.h"

namespace Emu {
    std::vector<EmulatorOption> GetDefaultOptions() {
        std::vector<EmulatorOption> options;

        options.emplace_back(
            "-gpu",
            "Set GPU Mode",
            "Set GPU emulation mode (host, swiftshader_indirect, angle_indirect, guest)",
            false,
            false,
            "e.g., host, swiftshader_indirect, ..."
        );

        options.emplace_back(
            "-memory",
            "Set Physical RAM (MBs)",
            "Physical RAM size in MBs",
            false,
            false,
            "e.g., 1024"
        );

        options.emplace_back(
            "-no-snapshot",
            "Full Boot",
            "Perform a full boot and do not auto-save on exit",
            true
        );

        options.emplace_back(
            "-no-audio",
            "Disable Audio",
            "Disable audio support",
            true
        );

        options.emplace_back(
            "-wipe-data",
            "Factory Reset",
            "Reset AVD to factory defaults (clears user data)",
            true
        );

        return options;
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

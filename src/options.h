//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_OPTIONS_H
#define EMU_LAUNCHER_OPTIONS_H

#include <string>
#include <vector>

namespace Emu {
    struct EmulatorOption {
        std::string Flag;
        std::string DisplayName;
        std::string Description;
        bool IsBoolean;
        bool Enabled;
        const char *Hint = nullptr;
        char ValueBuffer[256] = {};
    };

    std::vector<EmulatorOption> GetEmulatorOptions();

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options);
}

#endif //EMU_LAUNCHER_OPTIONS_H

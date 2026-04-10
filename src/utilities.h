//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef EMU_LAUNCHER_UTILITIES_H
#define EMU_LAUNCHER_UTILITIES_H
#include <format>
#include <string>

#include "imgui.h"

namespace CoreDeck {
    void ApplyCustomImGuiTheme();

    constexpr ImVec4 HexColor(const char *hex, float alpha = 1.0f) {
        auto hexToByte = [](const char hi, const char lo) -> float {
            auto charVal = [](const char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                return 0;
            };
            return static_cast<float>(charVal(hi) * 16 + charVal(lo)) / 255.0f;
        };

        if (hex[0] == '#') hex++;

        return {
            hexToByte(hex[0], hex[1]),
            hexToByte(hex[2], hex[3]),
            hexToByte(hex[4], hex[5]),
            alpha
        };
    }

    inline std::string IconWithLabel(const char *icon, const char *label) {
        return std::string{icon} + " " + label;
    }

    namespace Icons {
        constexpr const char *Play = "\xef\x81\x8b";
        constexpr const char *Stop = "\xef\x81\x8d";
        constexpr const char *Refresh = "\xef\x80\xa1";
        constexpr const char *Trash = "\xef\x87\xb8";
        constexpr const char *Circle = "\xef\x84\x91";
        constexpr const char *Desktop = "\xef\x84\x88";
        constexpr const char *Gear = "\xef\x80\x93";
        constexpr const char *Terminal = "\xef\x84\xa0";
        constexpr const char *Info = "\xef\x81\x9a";
        constexpr const char *Search = "\xef\x80\x82";
    }

    namespace Colors {
    }
}

#endif //EMU_LAUNCHER_UTILITIES_H

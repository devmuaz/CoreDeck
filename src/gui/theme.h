//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_THEME_H
#define COREDECK_THEME_H

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
}

#endif //COREDECK_THEME_H

//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_THEME_H
#define COREDECK_THEME_H

#include "imgui.h"

namespace CoreDeck {
    namespace Icons {
        constexpr const char *PLAY = "\xef\x81\x8b";
        constexpr const char *STOP = "\xef\x81\x8d";
        constexpr const char *REFRESH = "\xef\x80\xa1";
        constexpr const char *TRASH = "\xef\x87\xb8";
        constexpr const char *CIRCLE = "\xef\x84\x91";
        constexpr const char *DESKTOP = "\xef\x84\x88";
        constexpr const char *GEAR = "\xef\x80\x93";
        constexpr const char *TERMINAL = "\xef\x84\xa0";
        constexpr const char *INFO = "\xef\x81\x9a";
        constexpr const char *SEARCH = "\xef\x80\x82";
        constexpr const char *PLUS = "\xef\x81\xa7";
        constexpr const char *SORT_UP = "\xef\x83\x9e";
        constexpr const char *SORT_DOWN = "\xef\x83\x9d";
        constexpr const char *SORT = "\xef\x83\x9c";
        constexpr const char *TIMES = "\xef\x80\x8d";
        constexpr const char *MOBILE = "\xef\x8f\x8d";
        constexpr const char *TABLET = "\xef\x8f\xba";
        constexpr const char *TV = "\xef\x89\xac";
        constexpr const char *WATCH = "\xef\x80\x97";
        constexpr const char *CAR = "\xef\x86\xb9";
        constexpr const char *COPY = "\xef\x83\x85";
        constexpr const char *CHEVRON_LEFT = "\xef\x81\x93";
        constexpr const char *CHEVRON_RIGHT = "\xef\x81\x94";
    }

    namespace Colors {
        constexpr const char *WHITE = "#FFFFFF";
        constexpr const char *POSITIVE = "#33CC47";
        constexpr const char *POSITIVE_FILL = "#26B333";
        constexpr const char *NEGATIVE = "#E64D40";
        constexpr const char *NEGATIVE_STRONG = "#CC261F";
        constexpr const char *WARNING = "#D9B31A";
        constexpr const char *WARNING_STRONG = "#E6BF26";

        constexpr const char *ACCENT_PHONE = "#4FC3F7";
        constexpr const char *ACCENT_TABLET = "#22D3EE";
        constexpr const char *ACCENT_WEAR = "#F5A623";
        constexpr const char *ACCENT_TV = "#7E57C2";
        constexpr const char *ACCENT_INFO = "#4D9AFF";
        constexpr const char *ACCENT_INFO_SOFT = "#7AB8FF";

        constexpr const char *TEXT_PRIMARY = "#F2F2F2";
        constexpr const char *TEXT_MUTED = "#66666B";
        constexpr const char *TEXT_SUBTLE = "#A7A7AD";
        constexpr const char *TEXT_ON_DARK = "#CCCCCC";
        constexpr const char *TEXT_ON_BRIGHT = "#969696";
        constexpr const char *TEXT_HINT = "#CFCFD4";

        constexpr const char *SHADOW = "#000000";
        constexpr const char *SURFACE0 = "#0F0F12";
        constexpr const char *SURFACE1 = "#141417";
        constexpr const char *SURFACE2 = "#1A1A1C";
        constexpr const char *SURFACE3 = "#29292B";
        constexpr const char *SURFACE4 = "#2E2E33";

        constexpr const char *BORDER_SUBTLE = "#3F3F42";
        constexpr const char *BORDER = "#47474A";
        constexpr const char *BORDER_STRONG = "#4D4D4F";
        constexpr const char *BORDER_HOVER = "#5C5C5E";
    }

    void ApplyCustomImGuiTheme();

    constexpr ImVec4 HexColor(const char *hex, float alpha = 1.0F) {
        auto hexToByte = [](const char hi, const char lo) -> float {
            auto charVal = [](const char c) -> int {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f') {
                    return 10 + c - 'a';
                }
                if (c >= 'A' && c <= 'F') {
                    return 10 + c - 'A';
                }
                return 0;
            };
            return static_cast<float>((charVal(hi) * 16) + charVal(lo)) / 255.0F;
        };

        if (hex[0] == '#') {
            hex++;
        }

        return {
            hexToByte(hex[0], hex[1]),
            hexToByte(hex[2], hex[3]),
            hexToByte(hex[4], hex[5]),
            alpha
        };
    }
}

#endif // COREDECK_THEME_H

//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_THEME_H
#define COREDECK_THEME_H

#include "imgui.h"

namespace CoreDeck {
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
        constexpr const char *Plus = "\xef\x81\xa7";
        constexpr const char *SortUp = "\xef\x83\x9e";
        constexpr const char *SortDown = "\xef\x83\x9d";
        constexpr const char *Sort = "\xef\x83\x9c";
        constexpr const char *Times = "\xef\x80\x8d";
        constexpr const char *Mobile = "\xef\x8f\x8d";
        constexpr const char *Tablet = "\xef\x8f\xba";
        constexpr const char *Tv = "\xef\x89\xac";
        constexpr const char *Watch = "\xef\x80\x97";
        constexpr const char *Car = "\xef\x86\xb9";
        constexpr const char *Copy = "\xef\x83\x85";
        constexpr const char *ChevronLeft = "\xef\x81\x93";
        constexpr const char *ChevronRight = "\xef\x81\x94";
    }

    namespace Colors {
        constexpr const char *White = "#FFFFFF";
        constexpr const char *Positive = "#33CC47";
        constexpr const char *PositiveFill = "#26B333";
        constexpr const char *Negative = "#E64D40";
        constexpr const char *NegativeStrong = "#CC261F";
        constexpr const char *Warning = "#D9B31A";
        constexpr const char *WarningStrong = "#E6BF26";

        constexpr const char *AccentPhone = "#4FC3F7";
        constexpr const char *AccentTablet = "#22D3EE";
        constexpr const char *AccentWear = "#F5A623";
        constexpr const char *AccentTv = "#7E57C2";
        constexpr const char *AccentInfo = "#4D9AFF";
        constexpr const char *AccentInfoSoft = "#7AB8FF";

        constexpr const char *TextPrimary = "#F2F2F2";
        constexpr const char *TextMuted = "#66666B";
        constexpr const char *TextSubtle = "#A7A7AD";
        constexpr const char *TextOnDark = "#CCCCCC";
        constexpr const char *TextOnBright = "#969696";
        constexpr const char *TextHint = "#CFCFD4";

        constexpr const char *Shadow = "#000000";
        constexpr const char *Surface0 = "#0F0F12";
        constexpr const char *Surface1 = "#141417";
        constexpr const char *Surface2 = "#1A1A1C";
        constexpr const char *Surface3 = "#29292B";
        constexpr const char *Surface4 = "#2E2E33";

        constexpr const char *BorderSubtle = "#3F3F42";
        constexpr const char *Border = "#47474A";
        constexpr const char *BorderStrong = "#4D4D4F";
        constexpr const char *BorderHover = "#5C5C5E";
    }

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

#endif // COREDECK_THEME_H

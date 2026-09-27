//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_THEME_H
#define COREDECK_THEME_H

#include "imgui.h"

#include "../core/app_settings_types.h"

namespace CoreDeck {
    namespace Icons {
        constexpr const char *PLAY = "\xef\x81\x8b";
        constexpr const char *STOP = "\xef\x81\x8d";
        constexpr const char *REFRESH = "\xef\x80\xa1";
        constexpr const char *TRASH = "\xef\x87\xb8";
        constexpr const char *CIRCLE = "\xef\x84\x91";
        constexpr const char *DESKTOP = "\xef\x84\x88";
        constexpr const char *HARD_DRIVE = "\xef\x82\xa0";
        constexpr const char *GEAR = "\xef\x80\x93";
        constexpr const char *POWER_OFF = "\xef\x80\x91";
        constexpr const char *LIST = "\xef\x80\xba";
        constexpr const char *SLIDERS = "\xef\x87\x9e";
        constexpr const char *FILE_LINES = "\xef\x83\xb6";
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
        constexpr const char *COFFEE = "\xef\x83\xb4";
        constexpr const char *CHECK = "\xef\x80\x8c";
        constexpr const char *CHECK_CIRCLE = "\xef\x81\x98";
        constexpr const char *TIMES_CIRCLE = "\xef\x81\x97";
        constexpr const char *WARNING_TRIANGLE = "\xef\x81\xb1";
        constexpr const char *HEART_PULSE = "\xef\x88\x9e";
    }

    struct ColorToken {
        const char *const *Value;

        constexpr operator const char *() const { // NOLINT(google-explicit-constructor)
            return *Value;
        }
    };

    namespace Colors {
        extern const ColorToken WHITE;
        extern const ColorToken POSITIVE;
        extern const ColorToken POSITIVE_FILL;
        extern const ColorToken NEGATIVE;
        extern const ColorToken NEGATIVE_STRONG;
        extern const ColorToken WARNING;
        extern const ColorToken WARNING_STRONG;

        extern const ColorToken ACCENT_PHONE;
        extern const ColorToken ACCENT_TABLET;
        extern const ColorToken ACCENT_WEAR;
        extern const ColorToken ACCENT_TV;
        extern const ColorToken ACCENT_INFO;
        extern const ColorToken ACCENT_INFO_SOFT;

        extern const ColorToken STORAGE_AVD;
        extern const ColorToken STORAGE_SYSTEM_IMAGE;
        extern const ColorToken STORAGE_AVD_FILL;
        extern const ColorToken STORAGE_SYSTEM_IMAGE_FILL;

        extern const ColorToken TEXT_PRIMARY;
        extern const ColorToken TEXT_MUTED;
        extern const ColorToken TEXT_SUBTLE;
        extern const ColorToken TEXT_ON_DARK;
        extern const ColorToken TEXT_ON_BRIGHT;
        extern const ColorToken TEXT_HINT;

        extern const ColorToken SHADOW;
        extern const ColorToken SURFACE0;
        extern const ColorToken SURFACE1;
        extern const ColorToken SURFACE2;
        extern const ColorToken SURFACE3;
        extern const ColorToken SURFACE4;

        extern const ColorToken BORDER_SUBTLE;
        extern const ColorToken BORDER;
        extern const ColorToken BORDER_STRONG;
        extern const ColorToken BORDER_HOVER;
    }

    void ApplyCustomImGuiTheme(float dpiScale = 1.0F);

    void RefreshThemeColors();

    float GetDpiScale();

    bool IsLightColorScheme();

    void SetThemePreference(ThemePreference preference);

    bool SyncSystemColorScheme();

#ifdef __APPLE__
    void SetCocoaWindowAppearance(void *nativeWindow, bool light);
#endif

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

    constexpr ImVec4 HexColor(const ColorToken &color, const float alpha = 1.0F) {
        return HexColor(static_cast<const char *>(color), alpha);
    }
}

#endif // COREDECK_THEME_H

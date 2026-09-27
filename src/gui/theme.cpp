//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "theme.h"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <future>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace CoreDeck {
    namespace {
        float g_DpiScale = 1.0F;

        enum class ColorScheme : uint8_t {
            Dark,
            Light,
        };

        struct Palette {
            const char *White;
            const char *Positive;
            const char *PositiveFill;
            const char *Negative;
            const char *NegativeStrong;
            const char *Warning;
            const char *WarningStrong;
            const char *AccentPhone;
            const char *AccentTablet;
            const char *AccentWear;
            const char *AccentTv;
            const char *AccentInfo;
            const char *AccentInfoSoft;
            const char *StorageAvd;
            const char *StorageSystemImage;
            const char *StorageAvdFill;
            const char *StorageSystemImageFill;
            const char *TextPrimary;
            const char *TextMuted;
            const char *TextSubtle;
            const char *TextOnDark;
            const char *TextOnBright;
            const char *TextHint;
            const char *Shadow;
            const char *Surface0;
            const char *Surface1;
            const char *Surface2;
            const char *Surface3;
            const char *Surface4;
            const char *BorderSubtle;
            const char *Border;
            const char *BorderStrong;
            const char *BorderHover;
        };

        constexpr Palette DARK_PALETTE = {
            .White = "#FFFFFF",
            .Positive = "#33CC47",
            .PositiveFill = "#26B333",
            .Negative = "#E64D40",
            .NegativeStrong = "#CC261F",
            .Warning = "#D9B31A",
            .WarningStrong = "#E6BF26",
            .AccentPhone = "#4FC3F7",
            .AccentTablet = "#22D3EE",
            .AccentWear = "#F5A623",
            .AccentTv = "#7E57C2",
            .AccentInfo = "#4D9AFF",
            .AccentInfoSoft = "#7AB8FF",
            .StorageAvd = "#2980B9",
            .StorageSystemImage = "#27AE60",
            .StorageAvdFill = "#2980B9",
            .StorageSystemImageFill = "#27AE60",
            .TextPrimary = "#F2F2F2",
            .TextMuted = "#66666B",
            .TextSubtle = "#A7A7AD",
            .TextOnDark = "#CCCCCC",
            .TextOnBright = "#969696",
            .TextHint = "#CFCFD4",
            .Shadow = "#000000",
            .Surface0 = "#0F0F12",
            .Surface1 = "#141417",
            .Surface2 = "#1A1A1C",
            .Surface3 = "#29292B",
            .Surface4 = "#2E2E33",
            .BorderSubtle = "#3F3F42",
            .Border = "#47474A",
            .BorderStrong = "#4D4D4F",
            .BorderHover = "#5C5C5E",
        };

        constexpr Palette LIGHT_PALETTE = {
            .White = "#FFFFFF",
            .Positive = "#157A32",
            .PositiveFill = "#12702C",
            .Negative = "#C4312A",
            .NegativeStrong = "#9E221C",
            .Warning = "#8A6508",
            .WarningStrong = "#7A5A06",
            .AccentPhone = "#01579B",
            .AccentTablet = "#0E7490",
            .AccentWear = "#9A5B00",
            .AccentTv = "#5E35B1",
            .AccentInfo = "#1D4ED8",
            .AccentInfoSoft = "#1E40AF",
            .StorageAvd = "#1B6FBE",
            .StorageSystemImage = "#0E7A3A",
            .StorageAvdFill = "#3D98E6",
            .StorageSystemImageFill = "#34C56A",
            .TextPrimary = "#1C1C1F",
            .TextMuted = "#6E6E76",
            .TextSubtle = "#5E5E66",
            .TextOnDark = "#3A3A40",
            .TextOnBright = "#4A4A52",
            .TextHint = "#3F3F46",
            .Shadow = "#000000",
            .Surface0 = "#F6F6F7",
            .Surface1 = "#FFFFFF",
            .Surface2 = "#E8E8EB",
            .Surface3 = "#DCDCE0",
            .Surface4 = "#D0D0D4",
            .BorderSubtle = "#D4D4D8",
            .Border = "#C6C6CC",
            .BorderStrong = "#B4B4BA",
            .BorderHover = "#A1A1AA",
        };

        ColorScheme g_Scheme = ColorScheme::Dark;
        ThemePreference g_Preference = ThemePreference::Dark;
        int g_ProbeTicket = 0;
        int g_LaunchedTicket = 0;
        bool g_ProbeInFlight = false;
        std::future<bool> g_Probe;
        Palette g_Active = DARK_PALETTE;

        std::chrono::steady_clock::time_point &NextProbe() {
            static std::chrono::steady_clock::time_point nextProbe{};
            return nextProbe;
        }

        void AssignPalette(const Palette &palette) {
            g_Active = palette;
        }

        bool ApplyResolvedScheme(const ColorScheme scheme) {
            if (g_Scheme == scheme) {
                return false;
            }
            g_Scheme = scheme;
            AssignPalette(scheme == ColorScheme::Light ? LIGHT_PALETTE : DARK_PALETTE);
            return true;
        }

#ifdef _WIN32
        bool SystemPrefersDarkAppearance() {
            HKEY key = nullptr;
            if (RegOpenKeyExW(
                    HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0,
                    KEY_READ,
                    &key
                ) != ERROR_SUCCESS) {
                return false;
            }

            DWORD value = 1;
            DWORD size = sizeof(value);
            const LSTATUS status = RegQueryValueExW(
                key,
                L"AppsUseLightTheme",
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(&value),
                &size
            );
            RegCloseKey(key);
            return status == ERROR_SUCCESS && value == 0;
        }
#elif defined(__APPLE__)
        bool SystemPrefersDarkAppearance() {
            CFPreferencesAppSynchronize(CFSTR(".GlobalPreferences"));
            CFPropertyListRef value = CFPreferencesCopyValue(
                CFSTR("AppleInterfaceStyle"),
                CFSTR(".GlobalPreferences"),
                kCFPreferencesCurrentUser,
                kCFPreferencesAnyHost
            );
            if (value == nullptr) {
                return false;
            }

            bool dark = false;
            if (CFGetTypeID(value) == CFStringGetTypeID()) {
                dark = CFStringCompare(static_cast<CFStringRef>(value), CFSTR("Dark"), 0) == kCFCompareEqualTo;
            }
            CFRelease(value);
            return dark;
        }
#else
        std::string TrimCopy(const std::string &text) {
            std::size_t begin = 0;
            while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
                ++begin;
            }
            std::size_t end = text.size();
            while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
                --end;
            }
            return text.substr(begin, end - begin);
        }

        std::string LowerCopy(const std::string &text) {
            std::string lower = text;
            for (char &c: lower) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return lower;
        }

        bool ContainsDark(const std::string &text) {
            return LowerCopy(text).find("dark") != std::string::npos;
        }

        bool ReadCommand(const char *command, std::string &output) {
            FILE *pipe = popen(command, "r");
            if (pipe == nullptr) {
                return false;
            }
            output.clear();
            char buffer[256];
            while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            const int status = pclose(pipe);
            return status == 0 && !output.empty();
        }

        std::string ConfigDirectory() {
            if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] != '\0') {
                return xdg;
            }
            if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
                return std::string(home) + "/.config";
            }
            return {};
        }

        bool ValueAfterEquals(const std::string &line, std::string &value) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) {
                return false;
            }
            value = TrimCopy(line.substr(eq + 1));
            if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
                value.erase(value.begin());
            }
            if (!value.empty() && (value.back() == '"' || value.back() == '\'')) {
                value.pop_back();
            }
            return true;
        }

        bool KeyIs(const std::string &line, const char *key) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) {
                return false;
            }
            return TrimCopy(line.substr(0, eq)) == key;
        }

        bool GtkSettingsPreferDark(const std::string &path, bool &known) {
            std::ifstream file(path);
            if (!file) {
                return false;
            }

            bool sawFlag = false;
            bool flag = false;
            bool sawTheme = false;
            bool themeDark = false;
            std::string line;
            while (std::getline(file, line)) {
                const std::string trimmed = TrimCopy(line);
                if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
                    continue;
                }
                std::string value;
                if (!ValueAfterEquals(trimmed, value)) {
                    continue;
                }
                if (KeyIs(trimmed, "gtk-application-prefer-dark-theme")) {
                    const std::string lower = LowerCopy(value);
                    if (lower == "1" || lower == "true") {
                        sawFlag = true;
                        flag = true;
                    } else if (lower == "0" || lower == "false") {
                        sawFlag = true;
                        flag = false;
                    }
                } else if (KeyIs(trimmed, "gtk-theme-name")) {
                    sawTheme = true;
                    themeDark = ContainsDark(value);
                }
            }

            if (sawFlag) {
                known = true;
                return flag;
            }
            if (sawTheme) {
                known = true;
                return themeDark;
            }
            return false;
        }

        bool KdePrefersDark(const std::string &path, bool &known) {
            std::ifstream file(path);
            if (!file) {
                return false;
            }

            std::string line;
            while (std::getline(file, line)) {
                const std::string trimmed = TrimCopy(line);
                if (!KeyIs(trimmed, "ColorScheme")) {
                    continue;
                }
                std::string value;
                if (!ValueAfterEquals(trimmed, value)) {
                    continue;
                }
                known = true;
                return ContainsDark(value);
            }
            return false;
        }

        bool SystemPrefersDarkAppearance() {
            std::string colorScheme;
            if (ReadCommand("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", colorScheme)) {
                if (colorScheme.find("prefer-dark") != std::string::npos) {
                    return true;
                }
                if (colorScheme.find("prefer-light") != std::string::npos) {
                    return false;
                }
            }

            std::string preferDark;
            if (ReadCommand(
                    "gsettings get org.gnome.desktop.interface gtk-application-prefer-dark-theme 2>/dev/null",
                    preferDark
                ) &&
                preferDark.find("true") != std::string::npos) {
                return true;
            }

            std::string gtkTheme;
            if (ReadCommand("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", gtkTheme)) {
                return ContainsDark(gtkTheme);
            }

            const std::string config = ConfigDirectory();
            if (config.empty()) {
                return false;
            }

            bool known = false;
            if (const bool dark = GtkSettingsPreferDark(config + "/gtk-4.0/settings.ini", known); known) {
                return dark;
            }
            if (const bool dark = GtkSettingsPreferDark(config + "/gtk-3.0/settings.ini", known); known) {
                return dark;
            }
            if (const bool dark = KdePrefersDark(config + "/kdeglobals", known); known) {
                return dark;
            }
            return false;
        }
#endif

        bool PublishSystemScheme(const bool dark) {
            if (g_Preference != ThemePreference::System) {
                return false;
            }
            return ApplyResolvedScheme(dark ? ColorScheme::Dark : ColorScheme::Light);
        }
    }

    namespace Colors {
        const ColorToken WHITE{&g_Active.White};
        const ColorToken POSITIVE{&g_Active.Positive};
        const ColorToken POSITIVE_FILL{&g_Active.PositiveFill};
        const ColorToken NEGATIVE{&g_Active.Negative};
        const ColorToken NEGATIVE_STRONG{&g_Active.NegativeStrong};
        const ColorToken WARNING{&g_Active.Warning};
        const ColorToken WARNING_STRONG{&g_Active.WarningStrong};
        const ColorToken ACCENT_PHONE{&g_Active.AccentPhone};
        const ColorToken ACCENT_TABLET{&g_Active.AccentTablet};
        const ColorToken ACCENT_WEAR{&g_Active.AccentWear};
        const ColorToken ACCENT_TV{&g_Active.AccentTv};
        const ColorToken ACCENT_INFO{&g_Active.AccentInfo};
        const ColorToken ACCENT_INFO_SOFT{&g_Active.AccentInfoSoft};
        const ColorToken STORAGE_AVD{&g_Active.StorageAvd};
        const ColorToken STORAGE_SYSTEM_IMAGE{&g_Active.StorageSystemImage};
        const ColorToken STORAGE_AVD_FILL{&g_Active.StorageAvdFill};
        const ColorToken STORAGE_SYSTEM_IMAGE_FILL{&g_Active.StorageSystemImageFill};
        const ColorToken TEXT_PRIMARY{&g_Active.TextPrimary};
        const ColorToken TEXT_MUTED{&g_Active.TextMuted};
        const ColorToken TEXT_SUBTLE{&g_Active.TextSubtle};
        const ColorToken TEXT_ON_DARK{&g_Active.TextOnDark};
        const ColorToken TEXT_ON_BRIGHT{&g_Active.TextOnBright};
        const ColorToken TEXT_HINT{&g_Active.TextHint};
        const ColorToken SHADOW{&g_Active.Shadow};
        const ColorToken SURFACE0{&g_Active.Surface0};
        const ColorToken SURFACE1{&g_Active.Surface1};
        const ColorToken SURFACE2{&g_Active.Surface2};
        const ColorToken SURFACE3{&g_Active.Surface3};
        const ColorToken SURFACE4{&g_Active.Surface4};
        const ColorToken BORDER_SUBTLE{&g_Active.BorderSubtle};
        const ColorToken BORDER{&g_Active.Border};
        const ColorToken BORDER_STRONG{&g_Active.BorderStrong};
        const ColorToken BORDER_HOVER{&g_Active.BorderHover};
    }

    bool IsLightColorScheme() {
        return g_Scheme == ColorScheme::Light;
    }

    void SetThemePreference(const ThemePreference preference) {
        g_Preference = preference;
        ++g_ProbeTicket;
        NextProbe() = std::chrono::steady_clock::now() + std::chrono::seconds(1);

        if (preference == ThemePreference::System) {
            ApplyResolvedScheme(SystemPrefersDarkAppearance() ? ColorScheme::Dark : ColorScheme::Light);
            return;
        }
        ApplyResolvedScheme(preference == ThemePreference::Light ? ColorScheme::Light : ColorScheme::Dark);
    }

    bool SyncSystemColorScheme() {
        if (g_Preference != ThemePreference::System) {
            return false;
        }

        if (g_ProbeInFlight) {
            if (g_Probe.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return false;
            }
            const bool dark = g_Probe.get();
            g_ProbeInFlight = false;
            if (g_LaunchedTicket != g_ProbeTicket) {
                return false;
            }
            return PublishSystemScheme(dark);
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < NextProbe()) {
            return false;
        }
        NextProbe() = now + std::chrono::seconds(1);

#ifdef __linux__
        g_LaunchedTicket = g_ProbeTicket;
        g_Probe = std::async(std::launch::async, [] {
            return SystemPrefersDarkAppearance();
        });
        g_ProbeInFlight = true;
        return false;
#else
        return PublishSystemScheme(SystemPrefersDarkAppearance());
#endif
    }

    namespace {
        void ApplyThemeColors() {
            auto &c = ImGui::GetStyle().Colors;
            const bool light = IsLightColorScheme();

            // Dock tabs — inactive
            c[ImGuiCol_Tab] = HexColor(Colors::SHADOW, 0.0F);
            c[ImGuiCol_TabHovered] = HexColor(Colors::SHADOW, 0.0F);
            c[ImGuiCol_TabSelected] = HexColor(Colors::SHADOW, 0.0F);
            c[ImGuiCol_TabSelectedOverline] = HexColor(Colors::SHADOW, 0.0F);

            // Dock tabs — unfocused window
            c[ImGuiCol_TabDimmed] = HexColor(Colors::SHADOW, 0.0F);
            c[ImGuiCol_TabDimmedSelected] = HexColor(Colors::SHADOW, 0.0F);
            c[ImGuiCol_TabDimmedSelectedOverline] = HexColor(Colors::SHADOW, 0.0F);

            // Docking preview overlay
            c[ImGuiCol_DockingPreview] = HexColor(Colors::TEXT_PRIMARY, 0.20F);
            c[ImGuiCol_DockingEmptyBg] = HexColor(Colors::SURFACE0);

            // Window
            c[ImGuiCol_WindowBg] = HexColor(Colors::SURFACE0);
            c[ImGuiCol_ChildBg] = HexColor(Colors::SURFACE0);
            c[ImGuiCol_PopupBg] = HexColor(Colors::SURFACE1, 0.98F);
            c[ImGuiCol_ModalWindowDimBg] = HexColor(Colors::SHADOW, 0.55F);

            // Borders
            c[ImGuiCol_Border] = HexColor(Colors::SURFACE4);
            c[ImGuiCol_BorderShadow] = HexColor(Colors::SHADOW, 0.0F);

            // Text
            c[ImGuiCol_Text] = HexColor(Colors::TEXT_PRIMARY);
            c[ImGuiCol_TextDisabled] = HexColor(Colors::TEXT_MUTED);
            c[ImGuiCol_InputTextCursor] = HexColor(Colors::TEXT_PRIMARY);
            c[ImGuiCol_TextLink] = HexColor(Colors::ACCENT_INFO);

            // Headers
            c[ImGuiCol_Header] = HexColor(Colors::SURFACE3);
            c[ImGuiCol_HeaderHovered] = HexColor(Colors::SURFACE3);
            c[ImGuiCol_HeaderActive] = HexColor(Colors::SURFACE4);

            // Buttons
            c[ImGuiCol_Button] = HexColor(Colors::SURFACE2);
            c[ImGuiCol_ButtonHovered] = HexColor(light ? Colors::SURFACE3 : Colors::SURFACE4);
            c[ImGuiCol_ButtonActive] = HexColor(light ? Colors::SURFACE4 : Colors::SURFACE0);

            // Frame
            c[ImGuiCol_FrameBg] = HexColor(Colors::SURFACE1);
            c[ImGuiCol_FrameBgHovered] = HexColor(Colors::SURFACE3);
            c[ImGuiCol_FrameBgActive] = HexColor(Colors::SURFACE3);

            // Checkbox
            c[ImGuiCol_CheckMark] = HexColor(Colors::TEXT_PRIMARY);

            // Slider
            c[ImGuiCol_SliderGrab] = HexColor(Colors::TEXT_PRIMARY);
            c[ImGuiCol_SliderGrabActive] = HexColor(Colors::TEXT_ON_DARK);

            // Scrollbar
            c[ImGuiCol_ScrollbarBg] = HexColor(Colors::SURFACE0);
            c[ImGuiCol_ScrollbarGrab] = HexColor(Colors::SURFACE4);
            c[ImGuiCol_ScrollbarGrabHovered] = HexColor(Colors::BORDER);
            c[ImGuiCol_ScrollbarGrabActive] = HexColor(Colors::BORDER_HOVER);

            // Separator
            c[ImGuiCol_Separator] = HexColor(Colors::SURFACE2);
            c[ImGuiCol_SeparatorHovered] = HexColor(Colors::BORDER_STRONG);
            c[ImGuiCol_SeparatorActive] = HexColor(Colors::TEXT_MUTED);

            // Menu bar
            c[ImGuiCol_MenuBarBg] = HexColor(Colors::SURFACE0);

            // Title bar
            c[ImGuiCol_TitleBg] = HexColor(Colors::SURFACE0);
            c[ImGuiCol_TitleBgActive] = HexColor(Colors::SURFACE1);
            c[ImGuiCol_TitleBgCollapsed] = HexColor(Colors::SURFACE0);

            // Text selection
            c[ImGuiCol_TextSelectedBg] = light
                                             ? HexColor(Colors::ACCENT_INFO, 0.22F)
                                             : HexColor(Colors::BORDER_SUBTLE, 0.60F);

            // Resize grip
            c[ImGuiCol_ResizeGrip] = HexColor(Colors::SURFACE4, 0.25F);
            c[ImGuiCol_ResizeGripHovered] = HexColor(Colors::BORDER_STRONG, 0.65F);
            c[ImGuiCol_ResizeGripActive] = HexColor(Colors::TEXT_MUTED, 0.95F);
        }
    }

    void RefreshThemeColors() {
        if (IsLightColorScheme()) {
            ImGui::StyleColorsLight();
        } else {
            ImGui::StyleColorsDark();
        }
        ApplyThemeColors();
    }

    void ApplyCustomImGuiTheme(const float dpiScale) {
        g_DpiScale = dpiScale;
        auto &style = ImGui::GetStyle();
        const float s = dpiScale;

        style.WindowRounding = 6.0F * s;
        style.FrameRounding = 6.0F * s;
        style.GrabRounding = 6.0F * s;
        style.ScrollbarRounding = 6.0F * s;
        style.PopupRounding = 4.0F * s;
        style.FramePadding = ImVec2(8.0F * s, 8.0F * s);
        style.ItemSpacing = ImVec2(8.0F * s, 8.0F * s);
        style.ItemInnerSpacing = ImVec2(style.ItemInnerSpacing.x * s, style.ItemInnerSpacing.y * s);
        style.WindowPadding = ImVec2(8.0F * s, 8.0F * s);
        style.CellPadding = ImVec2(style.CellPadding.x * s, style.CellPadding.y * s);
        style.IndentSpacing = style.IndentSpacing * s;
        style.ScrollbarSize = 10.0F * s;
        style.GrabMinSize = style.GrabMinSize * s;
        style.FrameBorderSize = 0.6F;
        style.TabRounding = 0.0F;
        style.TabBarBorderSize = 0.0F;
        style.TabBorderSize = 0.0F;
        style.TabBarOverlineSize = 0.0F;

        RefreshThemeColors();
    }

    float GetDpiScale() {
        return g_DpiScale;
    }
}

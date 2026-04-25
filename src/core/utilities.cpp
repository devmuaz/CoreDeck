//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    void OpenUrl(const char *url) {
#if defined(_WIN32)
        ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        RunCommandArgs("/usr/bin/open", {url});
#else
        RunCommandArgs("xdg-open", {url});
#endif
    }

    std::uintmax_t GetDirectorySize(const std::string &path) {
        std::uintmax_t total = 0;
        std::error_code ec;
        for (const auto &entry: std::filesystem::recursive_directory_iterator(
                 path, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec)) {
                total += entry.file_size(ec);
            }
        }
        return total;
    }

    std::string FormatFileSize(const std::uintmax_t bytes) {
        constexpr double KB = 1024.0;
        constexpr double MB = KB * 1024.0;
        constexpr double GB = MB * 1024.0;

        char buf[64];
        const auto b = static_cast<double>(bytes);
        if (b >= GB) {
            std::snprintf(buf, sizeof(buf), "%.2f GB", b / GB);
            return buf;
        }
        if (b >= MB) {
            std::snprintf(buf, sizeof(buf), "%.1f MB", b / MB);
            return buf;
        }
        if (b >= KB) {
            std::snprintf(buf, sizeof(buf), "%.1f KB", b / KB);
            return buf;
        }
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return buf;
    }

    bool WipeAvdUserData(const std::string &avdPath) {
        if (!std::filesystem::exists(avdPath)) return false;

        static constexpr const char *wipeTargets[] = {
            "userdata-qemu.img",
            "userdata-qemu.img.qcow2",
            "cache.img",
            "cache.img.qcow2",
            "sdcard.img",
            "sdcard.img.qcow2",
            "snapshots",
        };

        bool wiped = false;
        std::error_code ec;
        for (const auto *name: wipeTargets) {
            const auto target = std::filesystem::path(avdPath) / name;
            if (std::filesystem::exists(target, ec)) {
                std::filesystem::remove_all(target, ec);
                if (!ec) wiped = true;
            }
        }
        return wiped;
    }
}

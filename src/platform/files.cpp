//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include "platform/files.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace CoreDeck::Platform {
    std::uintmax_t GetDirectorySize(const std::string &path) {
        std::uintmax_t total = 0;
        std::error_code ec;
        for (const auto &entry: std::filesystem::recursive_directory_iterator(
                 path,
                 std::filesystem::directory_options::skip_permission_denied,
                 ec
             )) {
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
            (void) std::snprintf(buf, sizeof(buf), "%.2f GB", b / GB);
            return buf;
        }
        if (b >= MB) {
            (void) std::snprintf(buf, sizeof(buf), "%.1f MB", b / MB);
            return buf;
        }
        if (b >= KB) {
            (void) std::snprintf(buf, sizeof(buf), "%.1f KB", b / KB);
            return buf;
        }
        (void) std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return buf;
    }
}

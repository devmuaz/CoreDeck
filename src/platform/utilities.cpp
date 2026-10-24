//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "process.h"
#include "utilities.h"

namespace CoreDeck::Platform {
    void OpenUrl(const char *url) {
#ifdef _WIN32
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


    void WipeSecret(void *data, const std::size_t size) {
        if (data == nullptr || size == 0) {
            return;
        }
#ifdef _WIN32
        SecureZeroMemory(data, size);
#else
        auto *bytes = static_cast<volatile unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index) {
            bytes[index] = 0;
        }
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    void WipeSecret(std::string &secret) {
        const std::size_t bytes = secret.capacity();
        if (bytes > 0) {
            secret.resize(bytes);
            WipeSecret(static_cast<void *>(secret.data()), secret.size());
        }
        secret.clear();
    }

    std::string LowerCopy(std::string value) {
        std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    bool ContainsIgnoreCase(const std::string &text, const std::string &needle) {
        if (needle.empty()) {
            return true;
        }
        return LowerCopy(text).find(LowerCopy(needle)) != std::string::npos;
    }
}

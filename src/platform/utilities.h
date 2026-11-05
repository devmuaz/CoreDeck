//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef COREDECK_UTILITIES_H
#define COREDECK_UTILITIES_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace CoreDeck::Platform {
    void OpenUrl(const char *url);

    std::uintmax_t GetDirectorySize(const std::string &path);

    std::string FormatFileSize(std::uintmax_t bytes);

    void WipeSecret(void *data, std::size_t size);

    void WipeSecret(std::string &secret);

    std::string LowerCopy(std::string value);

    bool ContainsIgnoreCase(const std::string &text, const std::string &needle);

    std::string Trim(std::string_view value);

    bool HasVisibleText(std::string_view text);

    void AppendLimited(std::string &target, std::string_view data, std::size_t limit);

    template<typename... Args>
    std::string StrConcat(Args &&...args) {
        std::string result;
        (result += ... += std::forward<Args>(args));
        return result;
    }
}

#endif // COREDECK_UTILITIES_H

//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef COREDECK_PLATFORM_STRINGS_H
#define COREDECK_PLATFORM_STRINGS_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace CoreDeck::Platform {
    std::string LowerCopy(std::string value);

    std::string UpperCopy(std::string value);

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

#endif // COREDECK_PLATFORM_STRINGS_H

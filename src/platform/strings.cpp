//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include "platform/strings.h"

#include <algorithm>
#include <cctype>

namespace CoreDeck::Platform {
    std::string LowerCopy(std::string value) {
        std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string UpperCopy(std::string value) {
        std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    bool ContainsIgnoreCase(const std::string &text, const std::string &needle) {
        if (needle.empty()) {
            return true;
        }
        return LowerCopy(text).find(LowerCopy(needle)) != std::string::npos;
    }

    std::string Trim(const std::string_view value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return std::string(value.substr(first, last - first + 1));
    }

    bool HasVisibleText(const std::string_view text) {
        return std::ranges::any_of(text, [](const unsigned char character) {
            return std::isspace(character) == 0;
        });
    }

    void AppendLimited(std::string &target, const std::string_view data, const std::size_t limit) {
        if (target.size() >= limit || data.empty()) {
            return;
        }
        const std::size_t count = std::min(data.size(), limit - target.size());
        target.append(data.substr(0, count));
    }
}

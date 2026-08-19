//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef COREDECK_UTILITIES_H
#define COREDECK_UTILITIES_H
#include <cstdint>
#include <string>

namespace CoreDeck {
    void OpenUrl(const char *url);

    std::uintmax_t GetDirectorySize(const std::string &path);

    std::string FormatFileSize(std::uintmax_t bytes);

    bool WipeAvdUserData(const std::string &avdPath);

    std::string LowerCopy(std::string value);

    bool ContainsIgnoreCase(const std::string &text, const std::string &needle);

    template<typename... Args>
    std::string StrConcat(Args &&...args) {
        std::string result;
        (result += ... += std::forward<Args>(args));
        return result;
    }
}

#endif // COREDECK_UTILITIES_H

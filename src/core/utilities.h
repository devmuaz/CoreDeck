//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef EMU_LAUNCHER_UTILITIES_H
#define EMU_LAUNCHER_UTILITIES_H
#include <cstdint>
#include <string>

namespace CoreDeck {
    void OpenUrl(const char *url);

    std::uintmax_t GetDirectorySize(const std::string &path);

    std::string FormatFileSize(std::uintmax_t bytes);

    bool WipeAvdUserData(const std::string &avdPath);

    template<typename... Args>
    std::string StrConcat(Args &&... args) {
        std::string result;
        (result += ... += std::forward<Args>(args));
        return result;
    }
}

#endif //EMU_LAUNCHER_UTILITIES_H

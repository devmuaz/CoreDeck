//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef EMU_LAUNCHER_UTILITIES_H
#define EMU_LAUNCHER_UTILITIES_H
#include <format>
#include <string>

namespace CoreDeck {
    void OpenUrl(const char *url);

    template<typename... Args>
    std::string StrConcat(Args &&... args) {
        std::string result;
        (result += ... += std::forward<Args>(args));
        return result;
    }
}

#endif //EMU_LAUNCHER_UTILITIES_H

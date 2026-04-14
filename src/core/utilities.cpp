//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include <string>

#include "utilities.h"

namespace CoreDeck {
    void OpenUrl(const char *url) {
#if defined(_WIN32)
        const std::string cmd = std::string("start ") + url;
#elif defined(__APPLE__)
        const std::string cmd = std::string("open ") + url;
#else
        const std::string cmd = std::string("xdg-open ") + url;
#endif
        std::system(cmd.c_str());
    }
}

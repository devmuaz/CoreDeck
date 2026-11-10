//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef COREDECK_PLATFORM_FILES_H
#define COREDECK_PLATFORM_FILES_H

#include <cstdint>
#include <string>

namespace CoreDeck::Platform {
    std::uintmax_t GetDirectorySize(const std::string &path);

    std::string FormatFileSize(std::uintmax_t bytes);
}

#endif // COREDECK_PLATFORM_FILES_H

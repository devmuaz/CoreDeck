//
// Created by AbdulMuaz Aqeel on 07/11/2026.
//

#ifndef COREDECK_PNG_H
#define COREDECK_PNG_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace CoreDeck::Platform {
    struct PngImage {
        int Width = 0;
        int Height = 0;
        std::vector<unsigned char> Rgba;
    };

    std::optional<PngImage> DecodePng(const unsigned char *data, std::size_t size);

    std::optional<PngImage> DecodePngFile(const std::string &path);
}

#endif // COREDECK_PNG_H

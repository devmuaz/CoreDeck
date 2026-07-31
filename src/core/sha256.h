//
// Created by AbdulMuaz Aqeel on 30/07/2026.
//

#ifndef COREDECK_SHA256_H
#define COREDECK_SHA256_H

#include <cstddef>
#include <string>

namespace CoreDeck {
    std::string Sha256Hex(const void *data, size_t size);

    std::string Sha256Hex(const std::string &data);

    std::string Sha256File(const std::string &path);

    bool EqualsIgnoreCaseHex(const std::string &a, const std::string &b);
}

#endif // COREDECK_SHA256_H

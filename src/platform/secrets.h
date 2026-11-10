//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#ifndef COREDECK_PLATFORM_SECRETS_H
#define COREDECK_PLATFORM_SECRETS_H

#include <cstddef>
#include <string>

namespace CoreDeck::Platform {
    void WipeSecret(void *data, std::size_t size);

    void WipeSecret(std::string &secret);
}

#endif // COREDECK_PLATFORM_SECRETS_H

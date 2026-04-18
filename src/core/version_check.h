//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_VERSION_CHECK_H
#define COREDECK_VERSION_CHECK_H

#include <optional>
#include <string>

namespace CoreDeck {
    std::optional<std::string> QueryRemoteNewerVersion();
}

#endif // COREDECK_VERSION_CHECK_H

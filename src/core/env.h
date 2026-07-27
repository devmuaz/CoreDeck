//
// Created by AbdulMuaz Aqeel on 24/07/2026.
//

#ifndef COREDECK_ENV_H
#define COREDECK_ENV_H

#include <string>
#include <vector>

namespace CoreDeck {
    struct EnvVar {
        std::string Name;
        std::string Value;
    };

    using EnvVars = std::vector<EnvVar>;
}

#endif // COREDECK_ENV_H

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_APP_APP_CONFIG_H
#define COREDECK_APP_APP_CONFIG_H

#include <utility>

#include "app/app_config_types.h"

namespace CoreDeck::App {
    AppConfig LoadAppConfig();

    bool SaveAppConfig(const AppConfig &config);

    template<typename Mutator>
    void UpdateAppConfig(Mutator &&mutate) {
        AppConfig config = LoadAppConfig();
        std::forward<Mutator>(mutate)(config);
        SaveAppConfig(config);
    }
}

#endif // COREDECK_APP_APP_CONFIG_H

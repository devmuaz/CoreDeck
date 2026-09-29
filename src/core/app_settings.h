//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_APP_SETTINGS_H
#define COREDECK_APP_SETTINGS_H

#include <utility>

#include "app_settings_types.h"

namespace CoreDeck {
    AppSettings LoadAppSettings();

    bool SaveAppSettings(const AppSettings &settings);

    template <typename Mutator>
    void UpdateAppSettings(Mutator &&mutate) {
        AppSettings settings = LoadAppSettings();
        std::forward<Mutator>(mutate)(settings);
        SaveAppSettings(settings);
    }
}

#endif // COREDECK_APP_SETTINGS_H

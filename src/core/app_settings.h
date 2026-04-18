//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_APP_SETTINGS_H
#define COREDECK_APP_SETTINGS_H

#include "app_settings_types.h"

namespace CoreDeck {
    AppSettings LoadAppSettings();

    void SaveAppSettings(const AppSettings &settings);
}

#endif //COREDECK_APP_SETTINGS_H

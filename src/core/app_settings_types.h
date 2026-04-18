//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_APP_SETTINGS_TYPES_H
#define COREDECK_APP_SETTINGS_TYPES_H

namespace CoreDeck {
    struct AppSettings {
        int SchemaVersion = 1;
        bool AutoScroll = true;
        bool ConfirmBeforeDeleteAvd = true;
        bool ShowAvdListPanel = true;
        bool ShowOptionsPanel = true;
        bool ShowDetailsPanel = true;
        bool ShowLogPanel = true;
    };
}

#endif //COREDECK_APP_SETTINGS_TYPES_H

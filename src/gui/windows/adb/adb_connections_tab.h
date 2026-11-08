//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_CONNECTIONS_TAB_H
#define COREDECK_ADB_CONNECTIONS_TAB_H

#include "../../context/context.h"

namespace CoreDeck::Gui::AdbWindow {
    void ConnectionsTab(Context &context, const Core::AdbDeviceSnapshot &snapshot);
}

#endif // COREDECK_ADB_CONNECTIONS_TAB_H

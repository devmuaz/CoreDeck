//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_DEVICES_WINDOW_H
#define COREDECK_ADB_DEVICES_WINDOW_H

#include "../../context/context.h"

namespace CoreDeck::Gui {
    void BuildDevicesWindow(Context &context, const Core::AdbDeviceSnapshot &snapshot);
}

#endif // COREDECK_ADB_DEVICES_WINDOW_H

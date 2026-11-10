//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_DEVICES_DIALOG_H
#define COREDECK_GUI_ADB_ADB_DEVICES_DIALOG_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    void BuildAdbDevicesDialog(Context &context, const Core::AdbDeviceSnapshot &snapshot);
}

#endif // COREDECK_GUI_ADB_ADB_DEVICES_DIALOG_H

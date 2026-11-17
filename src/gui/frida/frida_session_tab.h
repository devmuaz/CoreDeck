//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#ifndef COREDECK_GUI_FRIDA_FRIDA_SESSION_TAB_H
#define COREDECK_GUI_FRIDA_FRIDA_SESSION_TAB_H

#include "core/adb/adb_devices.h"
#include "gui/shell/context.h"

namespace CoreDeck::Gui::Frida {
    void BuildFridaSessionTab(Context &context, FridaDeviceSession &session, const Core::AdbDevice *device);

    void BuildFridaPrepareDialog(FridaState &workspace);
}

#endif // COREDECK_GUI_FRIDA_FRIDA_SESSION_TAB_H

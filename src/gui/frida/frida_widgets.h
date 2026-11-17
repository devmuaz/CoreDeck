//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#ifndef COREDECK_GUI_FRIDA_FRIDA_WIDGETS_H
#define COREDECK_GUI_FRIDA_FRIDA_WIDGETS_H

#include "core/adb/adb_devices.h"
#include "gui/shell/context.h"

namespace CoreDeck::Gui::Frida {
    void SpaceBelowToolbar();

    float PaneAboveLogHeight();

    void ShowFridaStatus(const FridaState &workspace, const FridaDeviceSession &session, const Core::AdbDevice *device);
}

#endif // COREDECK_GUI_FRIDA_FRIDA_WIDGETS_H

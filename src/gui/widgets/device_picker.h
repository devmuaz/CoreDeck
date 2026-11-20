//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#ifndef COREDECK_GUI_WIDGETS_DEVICE_PICKER_H
#define COREDECK_GUI_WIDGETS_DEVICE_PICKER_H

#include <string>

#include "core/adb/adb_service.h"

namespace CoreDeck::Gui {
    std::string DeviceLabel(const Core::AdbDevice &device);

    void DevicePicker(
        const char *id,
        std::string &selectedSerial,
        const Core::AdbDeviceSnapshot &snapshot,
        float room,
        const char *buttonLabel = nullptr,
        bool *buttonPressed = nullptr
    );
}

#endif // COREDECK_GUI_WIDGETS_DEVICE_PICKER_H

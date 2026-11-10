//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_WIDGETS_H
#define COREDECK_GUI_ADB_ADB_WIDGETS_H

#include <cstddef>
#include <optional>

#include "gui/shell/context.h"

namespace CoreDeck::Gui::Adb {
    void DevicePicker(Context &context, const Core::AdbDeviceSnapshot &snapshot, float room);

    void ShowTabStatus(
        const AdbState &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device,
        std::optional<std::size_t> capturedLines
    );
}

#endif // COREDECK_GUI_ADB_ADB_WIDGETS_H

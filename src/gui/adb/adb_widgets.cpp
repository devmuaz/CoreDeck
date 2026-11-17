//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "gui/adb/adb_widgets.h"

#include "gui/adb/adb_controller.h"
#include "gui/widgets/device_picker.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::Adb {
    void DevicePicker(Context &context, const Core::AdbDeviceSnapshot &snapshot, const float room) {
        auto &work = context.Workspaces.Adb;
        const std::string manageLabel = IconWithLabel(Icons::WIFI, Platform::Tr("Manage Wireless Devices"));
        bool manage = false;
        Gui::DevicePicker("##AdbDevice", work.SelectedSerial, snapshot, room, manageLabel.c_str(), &manage);
        if (manage) {
            work.ShowDevicesWindow = true;
        }
    }

    void ShowTabStatus(
        const AdbState &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device,
        const std::optional<std::size_t> capturedLines
    ) {
        AdbTabStatus status = VisibleTabStatus(workspace, session, snapshot, device);
        if (capturedLines.has_value()) {
            status.Message = Platform::TrFormat("{0} [{1} lines]", status.Message, std::to_string(*capturedLines));
        }
        StatusBar(status.Tone, status.Message.c_str());
    }
}

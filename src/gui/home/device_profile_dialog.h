//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#ifndef COREDECK_GUI_HOME_DEVICE_PROFILE_DIALOG_H
#define COREDECK_GUI_HOME_DEVICE_PROFILE_DIALOG_H

#include <string>

#include "gui/shell/context.h"
#include "gui/widgets/widgets.h"

namespace CoreDeck::Gui {
    DeviceCategory DeviceCategoryForText(const std::string &text);

    DeviceCategory DeviceCategoryForProfile(const Core::DeviceProfile &device);

    LabeledIconStyle DeviceFormFactorStyle(DeviceCategory category);

    std::string DeviceProfilePreviewLabel(const Core::DeviceProfile &device);

    void BuildDeviceProfileDialog(Context &context);
}

#endif // COREDECK_GUI_HOME_DEVICE_PROFILE_DIALOG_H

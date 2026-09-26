//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#ifndef COREDECK_DEVICE_PROFILE_WINDOW_H
#define COREDECK_DEVICE_PROFILE_WINDOW_H

#include <string>

#include "../context.h"
#include "../widgets.h"

namespace CoreDeck {
    DeviceCategory DeviceCategoryForText(const std::string &text);

    DeviceCategory DeviceCategoryForProfile(const DeviceProfile &device);

    LabeledIconStyle DeviceFormFactorStyle(DeviceCategory category);

    std::string DeviceProfilePreviewLabel(const DeviceProfile &device);

    void BuildDeviceProfileWindow(Context &context);
}

#endif // COREDECK_DEVICE_PROFILE_WINDOW_H

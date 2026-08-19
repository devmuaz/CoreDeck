//
// Created by AbdulMuaz Aqeel on 02/05/2026.
//

#ifndef COREDECK_DEVICE_PROFILE_WINDOW_H
#define COREDECK_DEVICE_PROFILE_WINDOW_H

#include <string>

#include "../context.h"

namespace CoreDeck {
    DeviceCategory DeviceCategoryForProfile(const DeviceProfile &device);

    std::string DeviceProfilePreviewLabel(const DeviceProfile &device);

    void BuildDeviceProfileWindow(Context &context);
}

#endif // COREDECK_DEVICE_PROFILE_WINDOW_H

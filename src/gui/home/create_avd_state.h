//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_GUI_HOME_CREATE_AVD_STATE_H
#define COREDECK_GUI_HOME_CREATE_AVD_STATE_H

#include <cstdint>
#include <vector>

#include "core/avd/avd_manager.h"
#include "core/avd/skin.h"
#include "core/avd/system_image.h"
#include "gui/shell/async_work.h"

namespace CoreDeck::Gui {
    enum class DeviceCategory : uint8_t {
        All,
        Phone,
        Tablet,
        Wear,
        Tv,
        Automotive,
        Desktop,
        Other,
    };

    struct AvdCreationState {
        std::vector<Core::SystemImage> SystemImages;
        std::vector<Core::DeviceProfile> DeviceProfiles;
        bool DeviceDefinitionsSkipped = false;
        std::vector<Core::Skin> Skins;
        Core::AvdCreationData CreationData;
        int SelectedSystemImage = 0;
        int SelectedDevice = 0;
        int PendingSelectedDevice = 0;
        int SelectedSkin = 0;
        int PendingSelectedSkin = 0;
        char SkinSearchFilter[128] = {};
        DeviceCategory SelectedDeviceCategory = DeviceCategory::Phone;
        char DeviceSearchFilter[128] = {};
        int SelectedGpuMode = 0;
        bool NameAutoFilled = true;
        bool DisplayNameAutoFilled = true;
        bool DeviceAutoFilled = true;
        bool SkinAutoFilled = true;
        int LastDeviceForSkinAuto = -1;

        AsyncLoad Prefetch;
        AsyncWork<bool> SystemImageRemoval;
    };
}

#endif // COREDECK_GUI_HOME_CREATE_AVD_STATE_H

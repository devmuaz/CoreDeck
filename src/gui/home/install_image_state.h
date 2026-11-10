//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_GUI_HOME_INSTALL_IMAGE_STATE_H
#define COREDECK_GUI_HOME_INSTALL_IMAGE_STATE_H

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "core/avd/system_image.h"
#include "core/sdk/sdk_manager.h"
#include "gui/shell/async_work.h"

namespace CoreDeck::Gui {
    enum class ImageCategory : uint8_t {
        All,
        PhoneTablet,
        Wear,
        Tv,
        Automotive,
        Desktop,
        Xr,
        Other,
    };

    struct ImageInstallationState {
        std::vector<Core::RemoteSystemImage> RemoteImages;
        int SelectedImage = -1;
        ImageCategory SelectedCategory = ImageCategory::PhoneTablet;
        char SearchFilter[128] = {};

        AsyncLoad Prefetch;

        std::atomic<bool> Installing{false};
        std::shared_ptr<Core::InstallProgressData> Progress;
        std::future<bool> InstallFuture;
        bool AwaitingLicenseConsent = false;
        std::atomic<bool> LicenseBusy{false};
        std::future<Core::SdkLicenseQuery> LicenseCheckFuture;
        std::future<bool> LicenseAcceptFuture;
        std::string PendingPackagePath;
        std::string LicenseError;
    };
}

#endif // COREDECK_GUI_HOME_INSTALL_IMAGE_STATE_H

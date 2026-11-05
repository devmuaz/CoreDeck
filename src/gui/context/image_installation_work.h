//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_IMAGE_INSTALLATION_WORK_H
#define COREDECK_IMAGE_INSTALLATION_WORK_H

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "core/sdk_manager.h"
#include "core/system_image.h"
#include "async_work.h"

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

    struct ImageInstallationWork {
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

#endif // COREDECK_IMAGE_INSTALLATION_WORK_H

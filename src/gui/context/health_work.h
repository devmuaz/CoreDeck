//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_HEALTH_WORK_H
#define COREDECK_HEALTH_WORK_H

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "core/health_check.h"
#include "core/sdk_manager.h"

namespace CoreDeck::Gui {
    struct HealthCheckWork {
        std::shared_ptr<Core::HealthCheckProgressData> Progress;
        std::atomic<bool> Busy{false};
        std::future<void> Future;
        std::string ObservedTools;
    };

    struct AcceptLicensesWork {
        std::atomic<bool> Busy{false};
        bool Accepted = false;
        std::future<bool> Future;
        std::string Error;
    };

    struct LicenseNotice {
        std::atomic<bool> Busy{false};
        bool Known = false;
        Core::LicenseStatus Status = Core::LicenseStatus::AllAccepted;
        std::string SdkManagerPath;
        std::uint32_t Epoch = 0;
        std::uint32_t PendingEpoch = 0;
        std::future<Core::LicenseStatus> Future;
    };
}

#endif // COREDECK_HEALTH_WORK_H

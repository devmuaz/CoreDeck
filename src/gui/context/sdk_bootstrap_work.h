//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_SDK_BOOTSTRAP_WORK_H
#define COREDECK_SDK_BOOTSTRAP_WORK_H

#include <atomic>
#include <future>
#include <memory>
#include <string>

#include "core/sdk_bootstrap.h"

namespace CoreDeck::Gui {
    struct SdkBootstrapWork {
        Core::BootstrapPlan Plan;
        std::shared_ptr<Core::BootstrapProgressData> Progress;
        std::atomic<bool> Busy{false};
        std::future<bool> Future;
        Core::BootstrapError LastError = Core::BootstrapError::None;
        std::string LastErrorDetail;
    };
}

#endif // COREDECK_SDK_BOOTSTRAP_WORK_H

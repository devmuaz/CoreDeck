//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_PACKAGES_TAB_H
#define COREDECK_ADB_PACKAGES_TAB_H

#include "../../context/context.h"

namespace CoreDeck::Gui::AdbWindow {
    void PackageFileTab(Context &context, const Core::AdbDevice *device);

    void ConfirmUninstall(Context &context);
}

#endif // COREDECK_ADB_PACKAGES_TAB_H

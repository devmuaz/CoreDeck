//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_PACKAGES_LIST_H
#define COREDECK_ADB_PACKAGES_LIST_H

#include "../../context/context.h"

namespace CoreDeck::Gui::AdbWindow {
    void OpenPackageList(Context &context, AdbDeviceSession &session);

    void BrowsePackages(Context &context);
}

#endif // COREDECK_ADB_PACKAGES_LIST_H

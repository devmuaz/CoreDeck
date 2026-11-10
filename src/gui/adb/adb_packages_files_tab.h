//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_PACKAGES_FILES_TAB_H
#define COREDECK_GUI_ADB_ADB_PACKAGES_FILES_TAB_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui::Adb {
    void BuildAdbPackagesFilesTab(Context &context, const Core::AdbDevice *device);

    void BuildUninstallPackageDialog(Context &context);
}

#endif // COREDECK_GUI_ADB_ADB_PACKAGES_FILES_TAB_H

//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_GUI_ADB_ADB_PACKAGE_LIST_DIALOG_H
#define COREDECK_GUI_ADB_ADB_PACKAGE_LIST_DIALOG_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui::Adb {
    void OpenAdbPackageListDialog(Context &context, AdbDeviceSession &session);

    void BuildAdbPackageListDialog(Context &context);
}

#endif // COREDECK_GUI_ADB_ADB_PACKAGE_LIST_DIALOG_H

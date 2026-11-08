//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_LOGCAT_TAB_H
#define COREDECK_ADB_LOGCAT_TAB_H

#include "../../context/context.h"

namespace CoreDeck::Gui::AdbWindow {
    void LogcatTab(Context &context, const Core::AdbDevice *device);

    void ConfirmClearDeviceLog(Context &context);
}

#endif // COREDECK_ADB_LOGCAT_TAB_H

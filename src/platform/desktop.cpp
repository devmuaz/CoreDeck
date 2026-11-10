//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include "platform/desktop.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "platform/process.h"

namespace CoreDeck::Platform {
    void OpenUrl(const char *url) {
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        RunCommandArgs("/usr/bin/open", {url});
#else
        RunCommandArgs("xdg-open", {url});
#endif
    }
}

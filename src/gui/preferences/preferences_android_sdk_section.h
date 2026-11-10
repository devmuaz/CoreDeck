//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#ifndef COREDECK_GUI_PREFERENCES_PREFERENCES_ANDROID_SDK_SECTION_H
#define COREDECK_GUI_PREFERENCES_PREFERENCES_ANDROID_SDK_SECTION_H

#include <cstddef>

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    void BuildPreferencesAndroidSdkSection(const Context &context, char *sdkPathBuffer, size_t bufferSize);

    void PollPreferencesAndroidSdkSection(Context &context);
}

#endif // COREDECK_GUI_PREFERENCES_PREFERENCES_ANDROID_SDK_SECTION_H

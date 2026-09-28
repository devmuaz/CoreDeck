//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#ifndef COREDECK_PREFERENCES_JDK_H
#define COREDECK_PREFERENCES_JDK_H

#include <cstddef>

#include "../context.h"

namespace CoreDeck {
    void DrawPreferencesJdkSection(Context &context, char *jdkPathBuffer, size_t bufferSize);

    void RequestPreferencesJdkScan();

    void PollPreferencesJdkWork(Context &context);
}

#endif // COREDECK_PREFERENCES_JDK_H

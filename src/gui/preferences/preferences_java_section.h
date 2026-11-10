//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#ifndef COREDECK_GUI_PREFERENCES_PREFERENCES_JAVA_SECTION_H
#define COREDECK_GUI_PREFERENCES_PREFERENCES_JAVA_SECTION_H

#include <cstddef>

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    void BuildPreferencesJavaSection(Context &context, char *jdkPathBuffer, size_t bufferSize);

    void RequestPreferencesJavaScan();

    void PollPreferencesJavaSection(Context &context);
}

#endif // COREDECK_GUI_PREFERENCES_PREFERENCES_JAVA_SECTION_H

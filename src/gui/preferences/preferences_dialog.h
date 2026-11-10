//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_GUI_PREFERENCES_PREFERENCES_DIALOG_H
#define COREDECK_GUI_PREFERENCES_PREFERENCES_DIALOG_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    void PreferencesSectionHeader(const char *title, const char *subtitle);

    void BuildPreferencesDialog(Context &context);
}

#endif // COREDECK_GUI_PREFERENCES_PREFERENCES_DIALOG_H

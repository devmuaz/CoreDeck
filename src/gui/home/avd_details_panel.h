//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_GUI_HOME_AVD_DETAILS_PANEL_H
#define COREDECK_GUI_HOME_AVD_DETAILS_PANEL_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    const char *SystemImageKindLabel(const Core::AvdInfo &avd);

    void BuildAvdDetailsPanel(Context &context);
}

#endif // COREDECK_GUI_HOME_AVD_DETAILS_PANEL_H

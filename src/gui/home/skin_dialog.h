//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#ifndef COREDECK_GUI_HOME_SKIN_DIALOG_H
#define COREDECK_GUI_HOME_SKIN_DIALOG_H

#include <string>

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    std::string SkinPreviewLabel(const Context &context);

    void BuildSkinDialog(Context &context);
}

#endif // COREDECK_GUI_HOME_SKIN_DIALOG_H
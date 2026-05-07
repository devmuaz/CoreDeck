//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#ifndef COREDECK_GUI_SKIN_H
#define COREDECK_GUI_SKIN_H

#include <string>

#include "../context.h"

namespace CoreDeck {
    std::string SkinPreviewLabel(const Context &context);

    void BuildSkinWindow(Context &context);
}

#endif // COREDECK_GUI_SKIN_H
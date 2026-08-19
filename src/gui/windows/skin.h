//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#ifndef COREDECK_SKIN_WINDOW_H
#define COREDECK_SKIN_WINDOW_H

#include <string>

#include "../context.h"

namespace CoreDeck {
    std::string SkinPreviewLabel(const Context &context);

    void BuildSkinWindow(Context &context);
}

#endif // COREDECK_SKIN_WINDOW_H
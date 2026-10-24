//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_INSTALL_IMAGE_WINDOW_H
#define COREDECK_INSTALL_IMAGE_WINDOW_H

#include "../context.h"
#include "gui/widgets.h"

namespace CoreDeck::Gui {
    LabeledIconStyle SystemImageTypeStyleForVariant(const std::string &variant);

    LabeledIconStyle SystemImageTypeStyleFor(const Core::SystemImage &img);

    LabeledIconStyle SystemImageTypeStyleFor(const Core::RemoteSystemImage &img);

    std::string SystemImageDisplayName(const std::string &apiLevel, const std::string &fallback);

    std::string SystemImagePreviewLabel(const Core::SystemImage &img);

    void OpenInstallImageDialog(Context &context);

    void BuildInstallImageWindow(Context &context);
}

#endif // COREDECK_INSTALL_IMAGE_WINDOW_H

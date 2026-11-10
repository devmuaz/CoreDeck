//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_GUI_HOME_INSTALL_IMAGE_DIALOG_H
#define COREDECK_GUI_HOME_INSTALL_IMAGE_DIALOG_H

#include "gui/shell/context.h"
#include "gui/widgets/widgets.h"

namespace CoreDeck::Gui {
    LabeledIconStyle SystemImageTypeStyleForVariant(const std::string &variant);

    LabeledIconStyle SystemImageTypeStyleFor(const Core::SystemImage &img);

    LabeledIconStyle SystemImageTypeStyleFor(const Core::RemoteSystemImage &img);

    std::string SystemImageDisplayName(const std::string &apiLevel, const std::string &fallback);

    std::string SystemImagePreviewLabel(const Core::SystemImage &img);

    void OpenInstallImageDialog(Context &context);

    void BuildInstallImageDialog(Context &context);
}

#endif // COREDECK_GUI_HOME_INSTALL_IMAGE_DIALOG_H

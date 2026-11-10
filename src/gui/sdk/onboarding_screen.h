//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#ifndef COREDECK_GUI_SDK_ONBOARDING_SCREEN_H
#define COREDECK_GUI_SDK_ONBOARDING_SCREEN_H

#include "gui/shell/context.h"

namespace CoreDeck::Gui {
    void BuildOnboardingScreen(Context &context);

    void OpenSdkSetupWizard(Context &context);

    void OpenCmdlineToolsInstall(Context &context);

    void OpenBuildToolsInstall(Context &context);

    void OpenPlatformToolsInstall(Context &context);
}

#endif // COREDECK_GUI_SDK_ONBOARDING_SCREEN_H

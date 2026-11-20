//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_widgets.h"

#include "gui/frida/frida_controller.h"
#include "gui/widgets/widgets.h"

namespace CoreDeck::Gui::Frida {
    void SpaceBelowToolbar() {
        ImGui::Spacing();
    }

    float PaneAboveLogHeight() {
        const float avail = ImGui::GetContentRegionAvail().y;
        const float gap = ImGui::GetStyle().ItemSpacing.y;
        return std::max(Em(8.0F), (avail - gap) * 0.55F);
    }

    void ShowFridaStatus(const FridaState &workspace, const FridaDeviceSession &session, const Core::AdbDevice *device) {
        const FridaTabStatus status = VisibleTabStatus(workspace, session, device);
        StatusBar(status.Tone, status.Message.c_str());
    }
}

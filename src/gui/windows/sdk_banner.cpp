//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "sdk_banner.h"
#include "../widgets.h"

namespace CoreDeck {
    void BuildSdkMissingBanner(Context &context) {
        if (context.Host.Sdk.IsFound) {
            context.UI.HideInvalidSdkPathBanner = false;
            return;
        }
        if (context.UI.HideInvalidSdkPathBanner) return;

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 0.0f));

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.32f, 0.18f, 0.10f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        ImGui::Begin("##SdkMissingBanner", nullptr, flags);

        ImGui::TextUnformatted(
            "No working Android SDK was detected (the emulator binary is missing or the path is invalid)."
        );
        ImGui::SameLine();
        if (PrimaryButton("Configure SDK", true)) {
            context.UI.ShowPreferences = true;
        }
        ImGui::SameLine();
        if (PrimaryButton("Dismiss for this session", true)) {
            context.UI.HideInvalidSdkPathBanner = true;
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

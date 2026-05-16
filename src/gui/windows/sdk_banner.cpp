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
        if (context.UI.HideInvalidSdkPathBanner) {
            return;
        }

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 0.0F));

        constexpr ImGuiWindowFlags FLAGS =
            WINDOW_AUTO_RESIZE_FLAGS |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.32F, 0.18F, 0.10F, 1.0F));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0F, 8.0F));
        ImGui::Begin("##SdkMissingBanner", nullptr, FLAGS);

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

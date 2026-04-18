//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "update.h"
#include "../../core/utilities.h"
#include "../theme.h"
#include "../widgets.h"

namespace CoreDeck {
    static void BuildUpToDateModal(Context &context) {
        if (!context.Updates.ShowUpToDateModal) {
            return;
        }

        if (!ImGui::IsPopupOpen("Up to date###CoreDeckUpdateOk")) {
            ImGui::OpenPopup("Up to date###CoreDeckUpdateOk");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal("Up to date###CoreDeckUpdateOk", &context.Updates.ShowUpToDateModal, flags)) {
            ImGui::TextWrapped("You're running the latest CoreDeck release.");
            ImGui::Spacing();
            ImGui::Text("Current: ");
            ImGui::SameLine(0, 0.0f);
            ImGui::TextColored(HexColor("#33CC47"), "v%s", COREDECK_VERSION);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (PrimaryButton("OK", true, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                context.Updates.ShowUpToDateModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void BuildUpdateNoticeWindow(Context &context) {
        BuildUpToDateModal(context);

        if (!context.Updates.ShowNewVersionModal) {
            return;
        }

        if (!ImGui::IsPopupOpen("Update Available###CoreDeckUpdate")) {
            ImGui::OpenPopup("Update Available###CoreDeckUpdate");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal("Update Available###CoreDeckUpdate", &context.Updates.ShowNewVersionModal, flags)) {
            ImGui::TextUnformatted("A newer CoreDeck release is available.");
            ImGui::Spacing();

            ImGui::TextUnformatted("Latest: ");
            ImGui::SameLine(0, 0.0f);
            ImGui::TextColored(HexColor("#33CC47"), "%s", context.Updates.LatestVersion.c_str());
            ImGui::TextUnformatted("Current: ");
            ImGui::SameLine(0, 0.0f);
            ImGui::TextColored(HexColor("#FF9F40"), "v%s", COREDECK_VERSION);
            ImGui::Spacing();
            ImGui::TextWrapped(
                "You will be taken to the CoreDeck website, where you can download and install the latest version."
            );
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            if (PositiveButton("Visit website", true, ImVec2(half, 0))) {
                OpenUrl(COREDECK_WEBSITE);
                context.Updates.ShowNewVersionModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (PrimaryButton("Later", true, ImVec2(half, 0))) {
                context.Updates.ShowNewVersionModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "../widgets.h"
#include "../context.h"
#include "../theme.h"
#include "about.h"

namespace CoreDeck {
    void BuildAboutWindow(Context &context) {
        if (context.UI.ShowAboutDialog && !ImGui::IsPopupOpen("About CoreDeck")) {
            ImGui::OpenPopup("About CoreDeck");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImVec2(Em(65.0F), 0), ImGuiCond_Appearing);


        if (ImGui::BeginPopupModal("About CoreDeck", &context.UI.ShowAboutDialog, WINDOW_NO_RESIZE_FLAGS)) {
            const auto centerCursor = [](const float textWidth) {
                ImGui::SetCursorPosX(
                    ((ImGui::GetContentRegionAvail().x - textWidth) * 0.5F) + ImGui::GetCursorStartPos().x
                );
            };

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            centerCursor(ImGui::CalcTextSize(COREDECK_TITLE).x);
            ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), COREDECK_TITLE);
            ImGui::PopFont();

            const std::string version = "Version " COREDECK_VERSION " (Build " COREDECK_BUILD_NUMBER ")";
            centerCursor(ImGui::CalcTextSize(version.c_str()).x);
            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", version.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto *const desc = COREDECK_DESCRIPTION;
            centerCursor(ImGui::CalcTextSize(desc).x);
            ImGui::TextUnformatted(desc);

            ImGui::Spacing();
            ImGui::Spacing();

            if (PropertyText("Author", COREDECK_VENDOR, true)) {
                OpenUrl(COREDECK_AUTHOR_WEBSITE);
            }
            PropertyText("License", "MIT");
            if (PropertyText("Website", "coredeck.dev", true)) {
                OpenUrl(COREDECK_WEBSITE);
            }
            if (PropertyText("GitHub", "github.com/devmuaz/CoreDeck", true)) {
                OpenUrl(COREDECK_GITHUB);
            }
            PropertyText("Built with", "C++20, Dear ImGui, GLFW, OpenGL");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            centerCursor(ImGui::CalcTextSize(COREDECK_COPYRIGHT).x);
            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", COREDECK_COPYRIGHT);

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
}

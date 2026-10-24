//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "../widgets.h"
#include "../context.h"
#include "../theme.h"
#include "about.h"
#include "../../core/constants.h"
#include "../../core/i18n.h"

namespace CoreDeck::Gui {
    void BuildAboutWindow(Context &context) {
        const std::string title = Core::TrWindow("About CoreDeck", "About CoreDeck");
        if (!context.UI.ShowAboutDialog && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        if (BeginCenteredModal(title.c_str(), &context.UI.ShowAboutDialog, ImVec2(Em(65.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
            const auto centerCursor = [](const float textWidth) {
                ImGui::SetCursorPosX(
                    ((ImGui::GetContentRegionAvail().x - textWidth) * 0.5F) + ImGui::GetCursorStartPos().x
                );
            };

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            centerCursor(ImGui::CalcTextSize(COREDECK_TITLE).x);
            ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), COREDECK_TITLE);
            ImGui::PopFont();

            const std::string version = Core::TrFormat("Version {0} (Build {1})", COREDECK_VERSION, COREDECK_BUILD_NUMBER);
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

            if (PropertyText(Core::Tr("Author"), COREDECK_VENDOR, true)) {
                Core::OpenUrl(Core::AUTHOR_WEBSITE_URL);
            }
            PropertyText(Core::Tr("License"), Core::Tr("MIT"));
            if (PropertyText(Core::Tr("Website"), Core::WEBSITE_URL, true)) {
                Core::OpenUrl(Core::WEBSITE_URL);
            }
            if (PropertyText(Core::Tr("GitHub"), Core::GITHUB_URL, true)) {
                Core::OpenUrl(Core::GITHUB_URL);
            }
            PropertyText(Core::Tr("Built with"), Core::Tr("C++20, Dear ImGui, GLFW, OpenGL"));

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

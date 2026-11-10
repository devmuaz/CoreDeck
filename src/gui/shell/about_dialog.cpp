//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "gui/shell/about_dialog.h"
#include "app/app_constants.h"
#include "gui/shell/context.h"
#include "gui/theme/theme.h"
#include "gui/widgets/widgets.h"
#include "platform/build_info.h"
#include "platform/desktop.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    void BuildAboutDialog(Context &context) {
        const std::string title = Platform::TrWindow("About CoreDeck", "About CoreDeck");
        if (!context.Dialogs.About.Open && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        if (BeginCenteredModal(title.c_str(), &context.Dialogs.About.Open, ImVec2(Em(65.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
            const auto centerCursor = [](const float textWidth) {
                ImGui::SetCursorPosX(
                    ((ImGui::GetContentRegionAvail().x - textWidth) * 0.5F) + ImGui::GetCursorStartPos().x
                );
            };

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            centerCursor(ImGui::CalcTextSize(Platform::BuildInfo::TITLE).x);
            ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), "%s", Platform::BuildInfo::TITLE);
            ImGui::PopFont();

            const std::string version = Platform::TrFormat("Version {0} (Build {1})", Platform::BuildInfo::VERSION, Platform::BuildInfo::BUILD_NUMBER);
            centerCursor(ImGui::CalcTextSize(version.c_str()).x);
            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", version.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto *const desc = Platform::BuildInfo::DESCRIPTION;
            centerCursor(ImGui::CalcTextSize(desc).x);
            ImGui::TextUnformatted(desc);

            ImGui::Spacing();
            ImGui::Spacing();

            if (PropertyText(Platform::Tr("Author"), Platform::BuildInfo::VENDOR, true)) {
                Platform::OpenUrl(App::AUTHOR_WEBSITE_URL);
            }
            PropertyText(Platform::Tr("License"), Platform::Tr("MIT"));
            if (PropertyText(Platform::Tr("Website"), App::WEBSITE_URL, true)) {
                Platform::OpenUrl(App::WEBSITE_URL);
            }
            if (PropertyText(Platform::Tr("GitHub"), App::GITHUB_URL, true)) {
                Platform::OpenUrl(App::GITHUB_URL);
            }
            PropertyText(Platform::Tr("Built with"), Platform::Tr("C++20, Dear ImGui, GLFW, OpenGL"));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            centerCursor(ImGui::CalcTextSize(Platform::BuildInfo::COPYRIGHT).x);
            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", Platform::BuildInfo::COPYRIGHT);

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
}

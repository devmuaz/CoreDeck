//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "gui/shell/update_dialog.h"
#include "app/app_constants.h"
#include "gui/theme/theme.h"
#include "gui/widgets/widgets.h"
#include "platform/build_info.h"
#include "platform/i18n.h"
#include "platform/desktop.h"
#include "platform/strings.h"

namespace CoreDeck::Gui {
    namespace {
        void RenderReleaseNotes(const std::string &notes) {
            if (notes.empty()) {
                return;
            }

            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ImGui::GetStyle().FrameRounding);
            ImGui::BeginChild("##ReleaseNotes", ImVec2(0, lineHeight * 12.0F), 1, ImGuiWindowFlags_HorizontalScrollbar);

            size_t start = 0;
            while (start <= notes.size()) {
                const size_t end = notes.find('\n', start);
                std::string line = notes.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                std::string trimmed = line;
                size_t leading = 0;
                while (leading < trimmed.size() && (trimmed.at(leading) == ' ' || trimmed.at(leading) == '\t')) {
                    ++leading;
                }
                trimmed.erase(0, leading);

                if (trimmed.empty()) {
                    ImGui::Spacing();
                } else if (trimmed.starts_with("### ")) {
                    ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", trimmed.substr(4).c_str());
                } else if (trimmed.starts_with("## ")) {
                    ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", trimmed.substr(3).c_str());
                } else if (trimmed.starts_with("# ")) {
                    ImGui::TextColored(HexColor(Colors::POSITIVE), "%s", trimmed.substr(2).c_str());
                } else if (trimmed.starts_with("- ") || trimmed.starts_with("* ")) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextWrapped(" %s", trimmed.substr(2).c_str());
                } else {
                    ImGui::TextWrapped("%s", trimmed.c_str());
                }

                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        void BuildUpToDateModal(Context &context) {
            if (!context.Dialogs.Update.UpToDateOpen) {
                return;
            }

            const std::string title = Platform::TrWindow("Up to date", "CoreDeckUpdateOk");
            if (BeginCenteredModal(title.c_str(), &context.Dialogs.Update.UpToDateOpen, ImVec2(Em(32.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
                ImGui::TextWrapped("%s", Platform::Tr("You're running the latest CoreDeck release."));
                ImGui::Spacing();
                ImGui::Text("%s", Platform::Tr("Current: "));
                ImGui::SameLine(0, 0.0F);
                ImGui::TextColored(HexColor(Colors::POSITIVE), "v%s", Platform::BuildInfo::VERSION);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (PrimaryButton(Platform::Tr("OK"), true, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    context.Dialogs.Update.UpToDateOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void BuildUpdateDialog(Context &context) {
        BuildUpToDateModal(context);

        if (!context.Dialogs.Update.Open) {
            return;
        }

        const std::string title = Platform::TrWindow("Update Available", "CoreDeckUpdate");
        if (BeginCenteredModal(title.c_str(), &context.Dialogs.Update.Open, ImVec2(Em(80.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
            ImGui::Spacing();
            ImGui::TextUnformatted(Platform::Tr("You're currently running on"));
            ImGui::SameLine();
            ImGui::TextColored(HexColor(Colors::WARNING), "v%s", Platform::BuildInfo::VERSION);
            ImGui::Spacing();

            RenderReleaseNotes(context.Dialogs.Update.LatestNotes);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float half = EqualButtonWidth(2);

            if (PositiveButton(Platform::Tr("Download"), true, ImVec2(half, 0))) {
                const std::string &latest = context.Dialogs.Update.LatestVersion;
                if (latest.find('-') != std::string::npos) {
                    const std::string releaseUrl = Platform::StrConcat(App::GITHUB_URL, "/releases/tag/", latest);
                    Platform::OpenUrl(releaseUrl.c_str());
                } else {
                    Platform::OpenUrl(App::WEBSITE_URL);
                }
                context.Dialogs.Update.Open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (PrimaryButton(Platform::Tr("Later"), true, ImVec2(half, 0))) {
                context.Dialogs.Update.Open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

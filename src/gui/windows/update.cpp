//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "update.h"
#include "../../core/constants.h"
#include "../../core/utilities.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/i18n.h"

namespace CoreDeck {
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
            if (!context.Updates.ShowUpToDateModal) {
                return;
            }

            const std::string title = TrWindow("Up to date", "CoreDeckUpdateOk");
            if (BeginCenteredModal(title.c_str(), &context.Updates.ShowUpToDateModal, ImVec2(Em(32.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
                ImGui::TextWrapped("%s", Tr("You're running the latest CoreDeck release."));
                ImGui::Spacing();
                ImGui::Text("%s", Tr("Current: "));
                ImGui::SameLine(0, 0.0F);
                ImGui::TextColored(HexColor(Colors::POSITIVE), "v%s", COREDECK_VERSION);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (PrimaryButton(Tr("OK"), true, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    context.Updates.ShowUpToDateModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void BuildUpdateNoticeWindow(Context &context) {
        BuildUpToDateModal(context);

        if (!context.Updates.ShowNewVersionModal) {
            return;
        }

        const std::string title = TrWindow("Update Available", "CoreDeckUpdate");
        if (BeginCenteredModal(title.c_str(), &context.Updates.ShowNewVersionModal, ImVec2(Em(80.0F), 0), WINDOW_NO_RESIZE_FLAGS)) {
            ImGui::Spacing();
            ImGui::TextUnformatted(Tr("You're currently running on"));
            ImGui::SameLine();
            ImGui::TextColored(HexColor(Colors::WARNING), "v%s", COREDECK_VERSION);
            ImGui::Spacing();

            RenderReleaseNotes(context.Updates.LatestNotes);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float half = EqualButtonWidth(2);

            if (PositiveButton(Tr("Download"), true, ImVec2(half, 0))) {
                OpenUrl(WEBSITE_URL);
                context.Updates.ShowNewVersionModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (PrimaryButton(Tr("Later"), true, ImVec2(half, 0))) {
                context.Updates.ShowNewVersionModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

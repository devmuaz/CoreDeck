//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include "imgui.h"

#include "update.h"
#include "../../core/utilities.h"
#include "../theme.h"
#include "../widgets.h"

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
                while (leading < trimmed.size() && (trimmed[leading] == ' ' || trimmed[leading] == '\t')) {
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

            if (!ImGui::IsPopupOpen("Up to date###CoreDeckUpdateOk")) {
                ImGui::OpenPopup("Up to date###CoreDeckUpdateOk");
            }

            const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            ImGui::SetNextWindowSize(ImVec2(Em(32.0F), 0), ImGuiCond_Appearing);

            if (RoundedBeginPopupModal("Up to date###CoreDeckUpdateOk", &context.Updates.ShowUpToDateModal, WINDOW_NO_RESIZE_FLAGS)) {
                ImGui::TextWrapped("You're running the latest CoreDeck release.");
                ImGui::Spacing();
                ImGui::Text("Current: ");
                ImGui::SameLine(0, 0.0F);
                ImGui::TextColored(HexColor(Colors::POSITIVE), "v%s", COREDECK_VERSION);
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
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImVec2(Em(80.0F), 0), ImGuiCond_Appearing);

        if (RoundedBeginPopupModal("Update Available###CoreDeckUpdate", &context.Updates.ShowNewVersionModal, WINDOW_NO_RESIZE_FLAGS)) {
            ImGui::Spacing();
            ImGui::TextUnformatted("You're currently running on");
            ImGui::SameLine();
            ImGui::TextColored(HexColor(Colors::WARNING), "v%s", COREDECK_VERSION);
            ImGui::Spacing();

            RenderReleaseNotes(context.Updates.LatestNotes);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5F;

            if (PositiveButton("Download", true, ImVec2(half, 0))) {
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

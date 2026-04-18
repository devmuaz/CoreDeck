//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include "imgui.h"

#include "avd_logs.h"
#include "../context.h"
#include "../theme.h"
#include "../widgets.h"

namespace CoreDeck {
    void BuildAvdLogsWindow(Context &context) {
        if (!context.UI.ShowLogPanel) return;

        ImGui::Begin("Output Log");

        std::shared_ptr<LogBuffer> log = nullptr;
        if (context.Catalog.SelectedAvd >= 0) {
            log = context.Host.Manager.GetLog(context.Catalog.Avds[context.Catalog.SelectedAvd].Name);
        }

        if (!log) ImGui::BeginDisabled();
        if (PrimaryButton(IconWithLabel(Icons::Trash, "Clear").c_str())) log->Clear();
        ImGui::SameLine();
        if (!log) ImGui::EndDisabled();

        constexpr float searchWidth = 300.0f;
        const float windowWidth = ImGui::GetWindowWidth();
        constexpr float rightPadding = 8.0f;
        ImGui::SetCursorPosX(windowWidth - searchWidth - rightPadding);
        ImGui::SetNextItemWidth(searchWidth);
        const std::string searchHint = std::string{Icons::Search} + " Search logs...";

        std::string currentSearch;
        if (context.Catalog.SelectedAvd >= 0) {
            const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
            currentSearch = context.Logs.PerAvdLogSearch[avdName];
        }

        char searchBuffer[256];
        strncpy(searchBuffer, currentSearch.c_str(), sizeof(searchBuffer) - 1);
        searchBuffer[sizeof(searchBuffer) - 1] = '\0';

        if (ImGui::InputTextWithHint("##search", searchHint.c_str(), searchBuffer, sizeof(searchBuffer))) {
            if (context.Catalog.SelectedAvd >= 0) {
                const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
                context.Logs.PerAvdLogSearch[avdName] = searchBuffer;
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("LogContent", ImVec2(0, 0), ImGuiChildFlags_None);

        if (context.Catalog.SelectedAvd < 0) {
            ImGui::TextDisabled("Select an AVD to view logs");
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        if (log) {
            const auto lines = log->GetLines();

            if (lines.empty()) {
                ImGui::TextDisabled("No available logs to view");
                ImGui::EndChild();
                ImGui::End();
                return;
            }

            std::string searchQuery;
            const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
            searchQuery = context.Logs.PerAvdLogSearch[avdName];
            const bool hasSearch = !searchQuery.empty();

            bool hasVisibleLines = false;
            for (const auto &line: lines) {
                // Filter lines based on search query (case-insensitive)
                if (hasSearch) {
                    std::string lowerLine = line;
                    std::string lowerQuery = searchQuery;
                    std::ranges::transform(lowerLine, lowerLine.begin(), tolower);
                    std::ranges::transform(lowerQuery, lowerQuery.begin(), tolower);

                    if (lowerLine.find(lowerQuery) == std::string::npos) {
                        continue;
                    }
                }

                ImGui::TextUnformatted(line.c_str());
                hasVisibleLines = true;
            }

            if (hasSearch && !hasVisibleLines) {
                ImGui::TextDisabled("No matching log entries found");
            }

            if (context.Logs.AutoScroll && log->HasNewContent() && !hasSearch) {
                ImGui::SetScrollHereY(1.0f);
                log->ResetNewContentFlag();
            }
        } else {
            const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
            ImGui::TextDisabled("Run the \"%s\" AVD to view logs", avdName.c_str());
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

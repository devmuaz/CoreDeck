//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#include "avd_logs.h"
#include "../context.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/log_filter.h"

namespace CoreDeck {
    namespace {
        struct PanelInputs {
            std::shared_ptr<LogBuffer> Log;
            std::string AvdName;
            bool HasSelection = false;
        };

        struct PanelView {
            LogFilterResult Filter;
            std::string Placeholder;
            bool HasContent = false;
        };

        struct SyncSelection {
            bool Active = false;
            int Start = 0;
            int End = 0;
        };

        PanelInputs ResolveInputs(Context &context) {
            PanelInputs inputs;
            if (context.Catalog.SelectedAvd >= 0) {
                inputs.HasSelection = true;
                inputs.AvdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
                inputs.Log = context.Host.Manager.GetLog(inputs.AvdName);
            }
            return inputs;
        }

        Context::LogViewState &ResolveViewState(Context &context, const std::string &avdName) {
            return context.Logs.PerAvdView[avdName];
        }

        PanelView BuildView(const PanelInputs &inputs, const Context::LogViewState &state) {
            PanelView view;
            if (!inputs.HasSelection) {
                view.Placeholder = "Select an AVD to view logs";
                return view;
            }
            if (!inputs.Log) {
                view.Placeholder = "Run the \"" + inputs.AvdName + "\" AVD to view logs";
                return view;
            }

            const auto lines = inputs.Log->GetLines();
            LogFilterOptions options;
            options.Query = state.Search;
            options.UseRegex = state.UseRegex;
            view.Filter = FilterLog(lines, options);
            view.HasContent = !view.Filter.Joined.empty();

            if (!view.HasContent) {
                view.Placeholder = lines.empty() ? "No available logs to view" : "No matching log entries found";
            }
            return view;
        }

        bool RenderToolbarButtons(const PanelInputs &inputs, const bool hasContent) {
            const bool disabled = !inputs.Log;
            if (disabled) {
                ImGui::BeginDisabled();
            }
            if (PrimaryButton(IconWithLabel(Icons::TRASH, "Clear").c_str())) {
                inputs.Log->Clear();
            }
            if (disabled) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();

            const bool canCopy = inputs.Log && hasContent;
            if (!canCopy) {
                ImGui::BeginDisabled();
            }
            const bool copyClicked = PrimaryButton(IconWithLabel(Icons::COPY, "Copy").c_str());
            if (!canCopy) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            return copyClicked;
        }

        bool RenderSearchBar(Context::LogViewState &state, const PanelView &view, const int matchCount, bool &queryChanged) {
            queryChanged = false;
            bool navChanged = false;

            constexpr float REGEX_TOGGLE_WIDTH = 32.0F;
            constexpr float NAV_BUTTON_WIDTH = 32.0F;
            constexpr float SEARCH_WIDTH = 260.0F;

            const bool hasQueryForWidth = !state.Search.empty();
            const bool regexInvalidForWidth = state.UseRegex && hasQueryForWidth && !view.Filter.RegexValid;
            const int displayedIndexForWidth = matchCount > 0 ? state.ActiveMatchIndex + 1 : 0;
            std::string counter;
            if (!hasQueryForWidth) {
                counter = "0 / 0";
            } else if (regexInvalidForWidth) {
                counter = "—";
            } else {
                counter = std::to_string(displayedIndexForWidth) + " / " + std::to_string(matchCount);
            }

            const ImGuiStyle &style = ImGui::GetStyle();
            const float counterWidth = ImGui::CalcTextSize(counter.c_str()).x;
            const float totalWidth = REGEX_TOGGLE_WIDTH + SEARCH_WIDTH + counterWidth + (NAV_BUTTON_WIDTH * 2.0F) + (style.ItemSpacing.x * 4.0F);
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - totalWidth);

            // Regex toggle
            if (ToggleButton(".*##RegexToggle", state.UseRegex, ImVec2(REGEX_TOGGLE_WIDTH, 0))) {
                queryChanged = true;
            }
            ImGui::SameLine();

            // Search field
            const bool regexInvalid = state.UseRegex && !state.Search.empty() && !view.Filter.RegexValid;
            char searchBuffer[256];
            std::strncpy(searchBuffer, state.Search.c_str(), sizeof(searchBuffer) - 1);
            searchBuffer[sizeof(searchBuffer) - 1] = '\0';

            const std::string hint = IconWithLabel(Icons::SEARCH, state.UseRegex ? "Regex" : "Search logs...");
            ImGui::SetNextItemWidth(SEARCH_WIDTH);
            if (regexInvalid) {
                ImGui::PushStyleColor(ImGuiCol_Border, HexColor(Colors::NEGATIVE));
            }
            if (regexInvalid) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F);
            }
            const bool edited = ImGui::InputTextWithHint("##search", hint.c_str(), searchBuffer, sizeof(searchBuffer));
            const bool enterPressed = ImGui::IsItemDeactivatedAfterEdit() && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
            if (regexInvalid) {
                ImGui::PopStyleVar();
            }
            if (regexInvalid) {
                ImGui::PopStyleColor();
            }
            if (regexInvalid && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Invalid regex: %s", view.Filter.RegexError.c_str());
            }
            if (edited) {
                state.Search = searchBuffer;
                queryChanged = true;
            }
            ImGui::SameLine();

            // Match counter
            ImGui::TextDisabled("%s", counter.c_str());
            ImGui::SameLine();

            // Prev / Next
            const bool canNav = matchCount > 0;
            if (!canNav) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button((std::string{Icons::CHEVRON_LEFT} + "##LogPrev").c_str(), ImVec2(NAV_BUTTON_WIDTH, 0))) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex - 1 + matchCount) % matchCount;
                navChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string{Icons::CHEVRON_RIGHT} + "##LogNext").c_str(), ImVec2(NAV_BUTTON_WIDTH, 0))) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex + 1) % matchCount;
                navChanged = true;
            }
            if (!canNav) {
                ImGui::EndDisabled();
            }

            if (canNav && enterPressed) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex + 1) % matchCount;
                navChanged = true;
            }

            return navChanged;
        }

        int CallbackSetSelection(ImGuiInputTextCallbackData *data) {
            const auto *selection = static_cast<SyncSelection *>(data->UserData);
            if (selection && selection->Active) {
                data->CursorPos = selection->End;
                data->SelectionStart = selection->Start;
                data->SelectionEnd = selection->End;
            }
            return 0;
        }

        std::size_t LineIndexFor(const std::string &joined, const std::size_t offset) {
            std::size_t line = 0;
            const std::size_t end = std::min(offset, joined.size());
            for (std::size_t i = 0; i < end; ++i) {
                if (joined[i] == '\n') {
                    ++line;
                }
            }
            return line;
        }

        ImGuiWindow *GetLogChildWindow() {
            return ImGui::FindWindowByID(ImGui::GetID("##LogText"));
        }

        bool ApplyScrollToLine(const int lineIndex) {
            if (lineIndex < 0) {
                return false;
            }
            ImGuiWindow *window = GetLogChildWindow();
            if (!window) {
                return false;
            }
            const float lineHeight = ImGui::GetTextLineHeight();
            const float regionH = window->InnerRect.GetHeight();
            const float targetY = static_cast<float>(lineIndex) * lineHeight;
            window->Scroll.y = std::max(0.0F, targetY - (regionH * 0.3F));
            return true;
        }

        bool ApplyScrollToBottom() {
            ImGuiWindow *w = GetLogChildWindow();
            if (!w) {
                return false;
            }
            w->Scroll.y = w->ScrollMax.y;
            return true;
        }

        void RenderLogBody(const PanelView &view, const SyncSelection &sync, const bool focusLog) {
            const std::string &display = view.HasContent ? view.Filter.Joined : view.Placeholder;
            std::vector<char> buffer(display.begin(), display.end());
            buffer.push_back('\0');

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly |
                                        ImGuiInputTextFlags_NoUndoRedo;
            if (sync.Active) {
                flags |= ImGuiInputTextFlags_CallbackAlways;
            }

            if (!view.HasContent) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, HexColor(Colors::ACCENT_INFO, 0.55F));
            if (focusLog) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextMultiline(
                "##LogText",
                buffer.data(),
                buffer.size(),
                ImVec2(-FLT_MIN, -FLT_MIN),
                flags,
                sync.Active ? CallbackSetSelection : nullptr,
                sync.Active ? const_cast<SyncSelection *>(&sync) : nullptr // NOLINT(cppcoreguidelines-pro-type-const-cast)
            );
            ImGui::PopStyleColor();
            if (!view.HasContent) {
                ImGui::PopStyleColor();
            }
        }

        void DriveAutoScroll(const PanelInputs &inputs, const Context &context, const PanelView &view, const bool hasQuery) {
            if (!inputs.Log || !context.Logs.AutoScroll || hasQuery || !view.HasContent) {
                return;
            }
            if (!inputs.Log->HasNewContent()) {
                return;
            }

            ApplyScrollToBottom();
            inputs.Log->ResetNewContentFlag();
        }
    }

    void BuildAvdLogsWindow(Context &context) {
        if (!context.UI.ShowLogPanel) {
            return;
        }

        ImGui::Begin("Output Log");

        const PanelInputs inputs = ResolveInputs(context);
        Context::LogViewState scratch{};
        Context::LogViewState &state = inputs.HasSelection ? ResolveViewState(context, inputs.AvdName) : scratch;

        PanelView view = BuildView(inputs, state);
        const int matchCount = static_cast<int>(view.Filter.Matches.size());

        if (state.ActiveMatchIndex >= matchCount) {
            state.ActiveMatchIndex = 0;
        }
        state.ActiveMatchIndex = std::max(state.ActiveMatchIndex, 0);

        const bool copyClicked = RenderToolbarButtons(inputs, view.HasContent);
        bool queryChanged = false;
        const bool navChanged = RenderSearchBar(state, view, matchCount, queryChanged);

        if (queryChanged) {
            state.ActiveMatchIndex = 0;
            view = BuildView(inputs, state);
            context.Logs.PendingScroll = !view.Filter.Matches.empty();
            if (!view.Filter.Matches.empty()) {
                context.Logs.PendingSyncFrames = 2;
            }
        }
        if (navChanged) {
            context.Logs.PendingScroll = true;
            context.Logs.PendingFocus = true;
            context.Logs.PendingSyncFrames = 2;
        }

        if (copyClicked && view.HasContent) {
            ImGui::SetClipboardText(view.Filter.Joined.c_str());
        }

        SyncSelection sync;
        int scrollLine = -1;
        const bool haveActiveMatch = !view.Filter.Matches.empty() && state.ActiveMatchIndex < static_cast<int>(view.Filter.Matches.size());
        if (haveActiveMatch && context.Logs.PendingSyncFrames > 0) {
            const auto &[StartOffset, EndOffset] = view.Filter.Matches[state.ActiveMatchIndex];
            sync.Active = true;
            sync.Start = static_cast<int>(StartOffset);
            sync.End = static_cast<int>(EndOffset);
        }
        if (haveActiveMatch && context.Logs.PendingScroll) {
            const auto &[StartOffset, _] = view.Filter.Matches[state.ActiveMatchIndex];
            scrollLine = static_cast<int>(LineIndexFor(view.Filter.Joined, StartOffset));
        }

        const bool focusLog = context.Logs.PendingFocus && haveActiveMatch;

        bool scrollApplied = true;
        if (scrollLine >= 0) {
            scrollApplied = ApplyScrollToLine(scrollLine);
        }
        if (scrollApplied) {
            context.Logs.PendingScroll = false;
        }

        context.Logs.PendingFocus = false;
        if (context.Logs.PendingSyncFrames > 0) {
            --context.Logs.PendingSyncFrames;
        }

        RenderLogBody(view, sync, focusLog);
        if (scrollLine < 0) {
            DriveAutoScroll(inputs, context, view, !state.Search.empty());
        }

        ImGui::End();
    }
}
//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "log_viewer.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#include "../theme.h"
#include "../utilities.h"
#include "widgets.h"
#include "platform/i18n.h"
#include "platform/log_filter.h"
#include "platform/utilities.h"

namespace CoreDeck::Gui {
    namespace {
        struct View {
            Platform::LogFilterResult Filter;
            std::string Placeholder;
            bool HasContent = false;
        };

        struct SyncSelection {
            bool Active = false;
            int Start = 0;
            int End = 0;
        };

        struct ShowInput {
            std::vector<std::string> Lines;
            const char *EmptyText = nullptr;
            bool Missing = false;
            bool PreferEmptyText = false;
            bool ClearEnabled = false;
            bool ShowPause = false;
            bool PauseEnabled = false;
            bool FollowTail = false;
            bool ContentChanged = false;
            bool RevealTop = false;
        };

        struct ShowResult {
            bool Clear = false;
            bool Pause = false;
            bool ScrolledToTail = false;
        };

        struct ToolbarResult {
            bool Clear = false;
            bool Pause = false;
            bool Copy = false;
        };

        std::uint64_t Fingerprint(const std::string &text) {
            if (text.empty()) {
                return 0;
            }
            std::uint64_t hash = 14695981039346656037ULL;
            for (const unsigned char c: text) {
                hash ^= c;
                hash *= 1099511628211ULL;
            }
            return hash == 0 ? 1 : hash;
        }

        std::vector<std::string> LinesOf(const std::string &text) {
            std::vector<std::string> lines;
            if (!Platform::HasVisibleText(text)) {
                return lines;
            }
            std::size_t begin = 0;
            while (begin < text.size()) {
                const std::size_t end = text.find('\n', begin);
                std::size_t stop = end == std::string::npos ? text.size() : end;
                if (stop > begin && text.at(stop - 1) == '\r') {
                    --stop;
                }
                lines.emplace_back(text.substr(begin, stop - begin));
                if (end == std::string::npos) {
                    break;
                }
                begin = end + 1;
            }
            return lines;
        }

        View BuildView(const ShowInput &input, const LogViewerState &state) {
            View view;
            if (input.Missing) {
                view.Placeholder = input.EmptyText != nullptr
                                       ? input.EmptyText
                                       : Platform::Tr("No available logs to view");
                return view;
            }

            Platform::LogFilterOptions filter;
            filter.Query = state.Search;
            filter.UseRegex = state.UseRegex;
            filter.LineFilter = state.Filter;
            filter.LineFilterRegex = state.FilterUseRegex;
            view.Filter = Platform::FilterLog(input.Lines, filter);
            view.HasContent = !view.Filter.Joined.empty();
            if (view.HasContent) {
                return view;
            }

            const bool filteredAway = !input.Lines.empty() && !state.Filter.empty() && view.Filter.LineFilterValid;
            if (filteredAway) {
                view.Placeholder = Platform::Tr("No matching log entries found");
            } else if (input.PreferEmptyText && input.EmptyText != nullptr) {
                view.Placeholder = input.EmptyText;
            } else {
                view.Placeholder = Platform::Tr("No available logs to view");
            }
            return view;
        }

        ToolbarResult RenderToolbar(const ShowInput &input, const bool paused, const bool canCopy) {
            ToolbarResult result;
            if (!input.ClearEnabled) ImGui::BeginDisabled();
            result.Clear = PrimaryButton(Icons::TRASH);
            if (!input.ClearEnabled) ImGui::EndDisabled();
            ImGui::SameLine();

            if (input.ShowPause) {
                if (!input.PauseEnabled) ImGui::BeginDisabled();
                const bool showPlay = !input.PauseEnabled || paused;
                result.Pause = PrimaryButton(showPlay ? Icons::PLAY : Icons::PAUSE);
                if (!input.PauseEnabled) ImGui::EndDisabled();
                ImGui::SameLine();
            }

            if (!canCopy) ImGui::BeginDisabled();
            result.Copy = PrimaryButton(Icons::COPY);
            if (!canCopy) ImGui::EndDisabled();
            ImGui::SameLine();
            return result;
        }

        std::string MatchCounter(const LogViewerState &state, const View &view, const int matchCount) {
            const bool hasQuery = !state.Search.empty();
            const bool invalid = state.UseRegex && hasQuery && !view.Filter.RegexValid;
            if (!hasQuery) {
                return "0 / 0";
            }
            if (invalid) {
                return "—";
            }
            const int shownIndex = matchCount > 0 ? state.ActiveMatchIndex + 1 : 0;
            return std::to_string(shownIndex) + " / " + std::to_string(matchCount);
        }

        float SharedQueryWidth(const std::string &counter) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float button = ImGui::GetFrameHeight();
            const float counterW = ImGui::CalcTextSize(counter.c_str()).x;
            const float chrome = (button * 4.0F) + counterW + (spacing * 6.0F);
            const float available = ImGui::GetWindowContentRegionMax().x - ImGui::GetCursorPosX() - chrome;
            return std::max(Em(8.0F), available * 0.5F);
        }

        bool MarkedField(
            const char *id,
            const char *hint,
            char *buffer,
            const std::size_t size,
            const float width,
            const bool invalid
        ) {
            ImGui::SetNextItemWidth(std::max(1.0F, width));
            StyleColor sc;
            StyleVar sv;
            if (invalid) {
                sc.Push(ImGuiCol_Border, HexColor(Colors::NEGATIVE));
                sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);
            }
            return ImGui::InputTextWithHint(id, hint, buffer, size);
        }

        bool RenderFilter(LogViewerState &state, const float fieldWidth) {
            ImGui::PushID("##filter");
            const float buttonSize = ImGui::GetFrameHeight();
            bool changed = false;
            if (ToggleButton(".*##Regex", state.FilterUseRegex, ImVec2(buttonSize, buttonSize))) {
                changed = true;
            }
            ImGui::SameLine();

            char buffer[256];
            CopyToBuffer(buffer, sizeof(buffer), state.Filter);
            const std::string hint = IconWithLabel(
                Icons::FILTER,
                state.FilterUseRegex ? Platform::Tr("Regex") : Platform::Tr("Filter logs...")
            );
            const Platform::LogQueryCheck check = Platform::CheckLogQuery(state.Filter, state.FilterUseRegex);
            const bool invalid = state.FilterUseRegex && !state.Filter.empty() && !check.Valid;
            if (MarkedField("##query", hint.c_str(), buffer, sizeof(buffer), fieldWidth, invalid)) {
                state.Filter = buffer;
                changed = true;
            }
            if (invalid && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Platform::TrFormat("Invalid regex: {0}", check.Error).c_str());
            }
            ImGui::PopID();
            return changed;
        }

        bool RenderSearch(
            LogViewerState &state,
            const View &view,
            const int matchCount,
            const float fieldWidth,
            bool &queryChanged
        ) {
            queryChanged = false;
            bool navigationChanged = false;
            const float buttonSize = ImGui::GetFrameHeight();
            const std::string counter = MatchCounter(state, view, matchCount);
            const bool hasQuery = !state.Search.empty();
            const bool invalid = state.UseRegex && hasQuery && !view.Filter.RegexValid;

            if (ToggleButton(".*##RegexToggle", state.UseRegex, ImVec2(buttonSize, buttonSize))) {
                queryChanged = true;
            }
            ImGui::SameLine();

            char search[256];
            CopyToBuffer(search, sizeof(search), state.Search);
            const std::string hint = IconWithLabel(
                Icons::SEARCH,
                state.UseRegex ? Platform::Tr("Regex") : Platform::Tr("Search logs...")
            );
            const bool edited = MarkedField("##search", hint.c_str(), search, sizeof(search), fieldWidth, invalid);
            const bool enter = ImGui::IsItemDeactivatedAfterEdit() && ImGui::IsKeyPressed(ImGuiKey_Enter, false);
            if (invalid && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Platform::TrFormat("Invalid regex: {0}", view.Filter.RegexError).c_str());
            }
            if (edited) {
                state.Search = search;
                queryChanged = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", counter.c_str());
            ImGui::SameLine();

            const bool canNavigate = matchCount > 0;
            if (!canNavigate) ImGui::BeginDisabled();
            if (ImGui::Button((std::string{Icons::CHEVRON_LEFT} + "##LogPrev").c_str(), ImVec2(buttonSize, buttonSize))) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex - 1 + matchCount) % matchCount;
                navigationChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string{Icons::CHEVRON_RIGHT} + "##LogNext").c_str(), ImVec2(buttonSize, buttonSize))) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex + 1) % matchCount;
                navigationChanged = true;
            }
            if (!canNavigate) ImGui::EndDisabled();
            if (canNavigate && enter) {
                state.ActiveMatchIndex = (state.ActiveMatchIndex + 1) % matchCount;
                navigationChanged = true;
            }
            return navigationChanged;
        }

        int SetSelection(ImGuiInputTextCallbackData *data) {
            const auto *selection = static_cast<SyncSelection *>(data->UserData);
            if (selection != nullptr && selection->Active) {
                data->CursorPos = selection->End;
                data->SelectionStart = selection->Start;
                data->SelectionEnd = selection->End;
            }
            return 0;
        }

        std::size_t LineIndex(const std::string &joined, const std::size_t offset) {
            return static_cast<std::size_t>(std::count(joined.begin(), joined.begin() + std::min(offset, joined.size()), '\n'));
        }

        ImGuiWindow *LogChild() {
            ImGuiWindow *parent = ImGui::GetCurrentWindow();
            const ImGuiID id = parent->GetID("##LogText");
            for (const auto *it = parent->DC.ChildWindows.end(); it != parent->DC.ChildWindows.begin();) {
                --it;
                if (*it != nullptr && (*it)->ChildId == id) {
                    return *it;
                }
            }
            return nullptr;
        }

        bool ScrollToLine(const int line) {
            ImGuiWindow *window = LogChild();
            if (window == nullptr || line < 0) {
                return false;
            }
            const float target = static_cast<float>(line) * ImGui::GetTextLineHeight();
            ImGui::SetScrollY(window, std::max(0.0F, target - (window->InnerRect.GetHeight() * 0.3F)));
            return true;
        }

        bool ScrollToBottom() {
            ImGuiWindow *window = LogChild();
            if (window == nullptr) {
                return false;
            }
            ImGui::SetScrollY(window, 1.0e9F);
            return true;
        }

        float BodyHeight(const LogViewerOptions &options) {
            if (options.MinBodyHeight <= 0.0F) {
                return -FLT_MIN;
            }
            return std::max(options.MinBodyHeight, ImGui::GetContentRegionAvail().y);
        }

        void RenderBody(const View &view, SyncSelection selection, const bool focus, const float height) {
            const std::string &display = view.HasContent ? view.Filter.Joined : view.Placeholder;
            std::vector buffer(display.begin(), display.end());
            buffer.push_back('\0');
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo | ImGuiInputTextFlags_WordWrap;
            if (selection.Active) flags |= ImGuiInputTextFlags_CallbackAlways;
            StyleColor sc;
            if (!view.HasContent) {
                sc.Push(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            sc.Push(ImGuiCol_TextSelectedBg, HexColor(Colors::ACCENT_INFO, 0.55F));
            if (focus) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextMultiline(
                "##LogText",
                buffer.data(),
                buffer.size(),
                ImVec2(-FLT_MIN, height),
                flags,
                selection.Active ? SetSelection : nullptr,
                selection.Active ? &selection : nullptr
            );
        }

        void ArmMatch(LogViewerState &state, const int matchCount) {
            state.ActiveMatchIndex = 0;
            state.PendingScroll = matchCount > 0;
            state.PendingSyncFrames = matchCount > 0 ? 2 : 0;
            if (matchCount > 0) {
                state.PendingTop = false;
            }
        }

        ShowResult Show(
            const char *id,
            const ShowInput &input,
            LogViewerState &state,
            const LogViewerOptions &options
        ) {
            ImGui::PushID(id);
            View view = BuildView(input, state);
            int matchCount = static_cast<int>(view.Filter.Matches.size());
            state.ActiveMatchIndex = std::clamp(state.ActiveMatchIndex, 0, std::max(0, matchCount - 1));

            const ToolbarResult toolbar = RenderToolbar(input, state.Paused, view.HasContent);
            const float fieldW = SharedQueryWidth(MatchCounter(state, view, matchCount));
            const bool filterEdited = RenderFilter(state, fieldW);
            if (filterEdited) {
                view = BuildView(input, state);
                matchCount = static_cast<int>(view.Filter.Matches.size());
            }
            ImGui::SameLine();
            bool queryChanged = false;
            const bool navigationChanged = RenderSearch(state, view, matchCount, fieldW, queryChanged);
            if (queryChanged) {
                view = BuildView(input, state);
                matchCount = static_cast<int>(view.Filter.Matches.size());
                ArmMatch(state, matchCount);
            }
            if (filterEdited) {
                ArmMatch(state, matchCount);
                if (state.Filter.empty()) {
                    state.PendingTop = false;
                } else if (matchCount == 0 && view.HasContent) {
                    state.PendingScroll = false;
                    state.PendingSyncFrames = 0;
                    state.PendingTop = true;
                }
            }
            if (input.ContentChanged) {
                ArmMatch(state, state.Search.empty() ? 0 : matchCount);
                if (state.Search.empty() && input.RevealTop && view.HasContent) {
                    state.PendingTop = true;
                }
            }
            if (navigationChanged) {
                state.PendingScroll = true;
                state.PendingFocus = true;
                state.PendingSyncFrames = 2;
            }
            if (toolbar.Copy && view.HasContent) {
                ImGui::SetClipboardText(view.Filter.Joined.c_str());
            }

            SyncSelection selection;
            int scrollLine = -1;
            const bool activeMatch = !view.Filter.Matches.empty() &&
                                     std::cmp_less(state.ActiveMatchIndex, view.Filter.Matches.size());
            if (activeMatch && state.PendingSyncFrames > 0) {
                const auto &[start, end] = view.Filter.Matches.at(state.ActiveMatchIndex);
                selection = {.Active = true, .Start = static_cast<int>(start), .End = static_cast<int>(end)};
            }
            if (activeMatch && state.PendingScroll) {
                scrollLine = static_cast<int>(LineIndex(
                    view.Filter.Joined,
                    view.Filter.Matches.at(state.ActiveMatchIndex).StartOffset
                ));
            }

            const bool focus = state.PendingFocus && activeMatch;
            state.PendingFocus = false;
            if (state.PendingSyncFrames > 0) --state.PendingSyncFrames;
            RenderBody(view, selection, focus, BodyHeight(options));

            ShowResult result;
            result.Clear = toolbar.Clear;
            result.Pause = toolbar.Pause;
            if (scrollLine >= 0) {
                if (ScrollToLine(scrollLine)) {
                    state.PendingScroll = false;
                    state.PendingTop = false;
                }
            } else if (state.PendingTop) {
                if (ScrollToLine(0)) {
                    state.PendingTop = false;
                }
            } else if (input.FollowTail && view.HasContent && ScrollToBottom()) {
                result.ScrolledToTail = true;
            }
            ImGui::PopID();
            return result;
        }
    }

    void LogViewer(
        const char *id,
        const std::shared_ptr<Platform::LogBuffer> &log,
        LogViewerState &state,
        const LogViewerOptions &options
    ) {
        ShowInput input;
        input.EmptyText = options.EmptyText;
        input.Missing = !log;
        input.ClearEnabled = static_cast<bool>(log);
        if (!options.Live && state.Paused) {
            state.Paused = false;
            state.PausedLines.clear();
        }
        input.ShowPause = options.AllowPause;
        input.PauseEnabled = options.Live && static_cast<bool>(log);
        if (log) {
            input.Lines = state.Paused ? state.PausedLines : log->GetLines();
            input.FollowTail = options.AutoScroll && !state.Paused && state.Search.empty() && state.Filter.empty() && log->HasNewContent();
        }
        const ShowResult result = Show(id, input, state, options);
        if (result.Clear && log) {
            log->Clear();
            state.PausedLines.clear();
        }
        if (result.Pause && log && options.Live) {
            state.Paused = !state.Paused;
            state.PausedLines = state.Paused ? log->GetLines() : std::vector<std::string>{};
        }
        if (result.ScrolledToTail && log) {
            log->ResetNewContentFlag();
        }
    }

    void LogViewer(
        const char *id,
        std::string &text,
        bool &present,
        LogViewerState &state,
        const LogViewerOptions &options
    ) {
        const std::uint64_t stamp = Fingerprint(text);
        const bool changed = stamp != state.TextStamp;
        state.TextStamp = stamp;

        ShowInput input;
        input.Lines = LinesOf(text);
        input.EmptyText = options.EmptyText;
        input.PreferEmptyText = true;
        input.ClearEnabled = present || !input.Lines.empty();
        input.ContentChanged = changed;
        input.RevealTop = changed && !options.AutoScroll && state.Search.empty() && state.Filter.empty();
        input.FollowTail = changed && options.AutoScroll && state.Search.empty() && state.Filter.empty();
        const ShowResult result = Show(id, input, state, options);
        if (result.Clear) {
            text.clear();
            present = false;
            state.Paused = false;
            state.PausedLines.clear();
            state.TextStamp = 0;
        }
    }
}

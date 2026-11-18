//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_session_tab.h"

#include <algorithm>
#include <memory>
#include <string>

#include "core/frida_release.h"
#include "gui/adb/adb_controller.h"
#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "platform/files.h"
#include "gui/widgets/log_viewer.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"
#include "platform/strings.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        std::string PrepareHint(const FridaDeviceSession &session) {
            const char *asset = session.ServerAsset.load();
            if (asset == nullptr || asset[0] == '\0') {
                return Platform::StrConcat("v", Core::FRIDA_VERSION);
            }
            return Platform::StrConcat("v", Core::FRIDA_VERSION, " · ", asset);
        }

        std::string ConsoleEmptyText() {
            return std::string(Platform::Tr("No Frida output")) + '\n' + Platform::Tr("Script logs, errors, and hook hits show up here.");
        }

        float Wider(const std::string &first, const std::string &second) {
            return std::max(ActionWidth(first.c_str()), ActionWidth(second.c_str()));
        }

        bool LabeledButton(const bool negative, const std::string &label, const bool enabled, const float width) {
            if (negative) {
                return NegativeButton(label.c_str(), enabled, ImVec2(width, 0.0F));
            }
            return PrimaryButton(label.c_str(), enabled, ImVec2(width, 0.0F));
        }

        void SessionToolbar(Context &context, FridaDeviceSession &session, const bool usable) {
            const bool working = session.Working.load();
            const bool ready = session.Phase == FridaServerPhase::Ready;
            const std::string startLabel = IconWithLabel(Icons::PLAY, Platform::Tr("Start"));
            const std::string stopLabel = IconWithLabel(Icons::STOP, Platform::Tr("Stop"));
            const std::string existingLabel = IconWithLabel(Icons::LINK, Platform::Tr("Use Existing"));
            const std::string refreshLabel = IconWithLabel(Icons::REFRESH, Platform::Tr("Refresh"));
            const std::string attachLabel = IconWithLabel(Icons::PLUG, Platform::Tr("Attach"));
            const std::string detachLabel = IconWithLabel(Icons::LINK_SLASH, Platform::Tr("Detach"));
            const std::string spawnLabel = IconWithLabel(Icons::ROCKET, Platform::Tr("Spawn"));
            const std::string resumeLabel = IconWithLabel(Icons::PLAY, Platform::Tr("Resume"));

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float serverW = Wider(startLabel, stopLabel);
            const float existingW = ActionWidth(existingLabel.c_str());
            const float refreshW = ActionWidth(refreshLabel.c_str());
            const float attachW = Wider(attachLabel, detachLabel);
            const float spawnW = Wider(spawnLabel, resumeLabel);
            const float fixed = serverW + existingW + refreshW + attachW + spawnW + (spacing * 6.0F);
            const float fields = std::max(2.0F, ImGui::GetContentRegionAvail().x - fixed);
            const float filterW = std::max(1.0F, fields * 0.5F);
            const float packageW = std::max(1.0F, fields - filterW);

            const bool serverUp = ready || working;
            if (LabeledButton(serverUp, serverUp ? stopLabel : startLabel, serverUp || usable, serverW)) {
                if (serverUp) {
                    StopFridaSession(context, session);
                } else {
                    StartManagedFrida(context, session);
                }
            }
            ImGui::SameLine(0.0F, spacing);
            if (PickerButton(existingLabel.c_str(), usable && !working && !ready, ImVec2(existingW, 0.0F))) {
                UseExistingFrida(context, session);
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(refreshLabel.c_str(), ready, ImVec2(refreshW, 0.0F))) {
                RefreshFridaProcesses(context, session);
            }
            ImGui::SameLine(0.0F, spacing);
            if (LabeledButton(session.Attached, session.Attached ? detachLabel : attachLabel, session.Attached || (ready && session.SelectedProcess >= 0), attachW)) {
                if (session.Attached) {
                    DetachFrida(context, session);
                } else {
                    AttachSelectedFridaProcess(context, session);
                }
            }
            ImGui::SameLine(0.0F, spacing);
            SearchField("##FridaProcessFilter", Platform::Tr("Filter processes"), session.ProcessFilter, sizeof(session.ProcessFilter), filterW);
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(packageW);
            const std::string packageHint = IconWithLabel(Icons::MOBILE, Platform::Tr("Package name"));
            ImGui::InputTextWithHint("##FridaSpawnPackage", packageHint.c_str(), session.SpawnPackage, sizeof(session.SpawnPackage));
            HoverTooltip(Platform::Tr("Package name"));
            ImGui::SameLine(0.0F, spacing);
            const bool suspended = session.SuspendedPid != 0;
            const bool spawnClicked = suspended
                                          ? PositiveButton(resumeLabel.c_str(), true, ImVec2(spawnW, 0.0F))
                                          : PrimaryButton(spawnLabel.c_str(), ready && session.SpawnPackage[0] != '\0', ImVec2(spawnW, 0.0F));
            if (spawnClicked) {
                if (suspended) {
                    ResumeFridaSpawn(context, session);
                } else {
                    SpawnFridaPackage(context, session);
                }
            }
        }

        void ProcessTable(FridaDeviceSession &session) {
            const PickerTableStyle tableStyle;
            const float listHeight = PaneAboveLogHeight();
            const bool tableOpen = BeginPickerTable("##FridaProcessFrame", "##FridaProcesses", 2, listHeight);
            if (tableOpen) {
                const std::string pidColumn = Platform::StrConcat("  ", Platform::Tr("PID"));
                ImGui::TableSetupColumn(pidColumn.c_str(), ImGuiTableColumnFlags_WidthFixed, Em(8.0F));
                ImGui::TableSetupColumn(Platform::Tr("Name"), ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableHeadersRow();
                for (int index = 0; std::cmp_less(index, session.Processes.size()); ++index) {
                    const FridaProcessRow &process = session.Processes.at(static_cast<std::size_t>(index));
                    if (session.ProcessFilter[0] != '\0' && !Platform::ContainsIgnoreCase(process.Name, session.ProcessFilter)) {
                        continue;
                    }
                    const bool selected = session.SelectedProcess == index;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string label = Platform::StrConcat("  ", std::to_string(process.Pid), "##FridaProcess", std::to_string(index));
                    if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        session.SelectedProcess = index;
                        CopyToBuffer(session.SpawnPackage, sizeof(session.SpawnPackage), process.Name);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(process.Name.c_str());
                }
            }
            EndPickerTable(tableOpen);
        }

        void SessionConsole(FridaDeviceSession &session) {
            const std::string empty = ConsoleEmptyText();
            LogViewer(
                "FridaConsole",
                session.Console,
                session.HasConsole,
                session.ConsoleView,
                {
                    .EmptyText = empty.c_str(),
                    .AutoScroll = true,
                }
            );
        }
    }

    void BuildFridaSessionTab(Context &context, FridaDeviceSession &session, const Core::AdbDevice *device) {
        SessionToolbar(context, session, Adb::DeviceUsable(device));
        SpaceBelowToolbar();
        ProcessTable(session);
        SessionConsole(session);
    }

    void BuildFridaPrepareDialog(FridaState &workspace) {
        const auto preparing = [](const std::unique_ptr<FridaDeviceSession> &item) {
            return item != nullptr && item->Mode == Core::FridaServerMode::Managed && item->Working.load();
        };
        FridaDeviceSession *session = nullptr;
        const auto selected = workspace.Sessions.find(workspace.SelectedSerial);
        if (selected != workspace.Sessions.end() && preparing(selected->second)) {
            session = selected->second.get();
        } else {
            for (const auto &entry: workspace.Sessions) {
                if (preparing(entry.second)) {
                    session = entry.second.get();
                    break;
                }
            }
        }
        const std::string title = Platform::TrWindow("Preparing frida-server", "FridaPrepareDialog");
        if (session == nullptr && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }
        const float dialogW = Em(42.0F);
        ImGui::SetNextWindowSizeConstraints(ImVec2(dialogW, 0.0F), ImVec2(dialogW, FLT_MAX));
        if (!BeginCenteredModal(title.c_str(), nullptr, ImVec2(dialogW, 0.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            return;
        }
        if (session == nullptr) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const std::uint64_t total = session->Total.load();
        const std::uint64_t received = session->Received.load();
        const bool cancelRequested = session->Cancel.load();
        const char *activity = Platform::Tr("Installing Frida Server...");
        if (cancelRequested) {
            activity = Platform::Tr("Cancelling...");
        } else if (total > 0) {
            activity = Platform::Tr("Downloading Frida Server...");
        }
        const float fraction = total > 0
                                   ? std::clamp(static_cast<float>(received) / static_cast<float>(total), 0.0F, 1.0F)
                                   : static_cast<float>(-ImGui::GetTime());

        ImGui::Spacing();
        const float contentX = ImGui::GetCursorPosX();
        const float contentW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float textW = ImGui::CalcTextSize(activity).x;
        ImGui::SetCursorPosX(contentX + std::max(0.0F, (contentW - textW) * 0.5F));
        ImGui::TextUnformatted(activity);

        const std::string hint = PrepareHint(*session);
        const float hintW = ImGui::CalcTextSize(hint.c_str()).x;
        ImGui::SetCursorPosX(contentX + std::max(0.0F, (contentW - hintW) * 0.5F));
        ImGui::TextDisabled("%s", hint.c_str());

        ImGui::Spacing();
        ImGui::ProgressBar(fraction, ImVec2(contentW, 0.0F));

        if (total > 0) {
            ImGui::Spacing();
            const std::string detail = Platform::StrConcat(Platform::FormatFileSize(received), " / ", Platform::FormatFileSize(total));
            ImGui::TextDisabled("%s", detail.c_str());
        }

        ImGui::Spacing();
        if (NegativeButton(cancelRequested ? Platform::Tr("Cancelling...") : Platform::Tr("Cancel"), !cancelRequested, ImVec2(contentW, 0.0F))) {
            session->Cancel.store(true);
        }
        ImGui::EndPopup();
    }
}

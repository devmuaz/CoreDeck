#include "gui/adb/adb_workspace.h"
#include "gui/adb/adb_connections_tab.h"
#include "gui/adb/adb_controller.h"
#include "gui/adb/adb_devices_dialog.h"
#include "gui/adb/adb_logcat_tab.h"
#include "gui/adb/adb_package_list_dialog.h"
#include "gui/adb/adb_packages_files_tab.h"
#include "gui/adb/adb_shell_tab.h"
#include "gui/adb/adb_widgets.h"

#include "gui/sdk/onboarding_screen.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void WorkspaceBody(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            auto &workspace = context.Workspaces.Adb;
            const char *tabs[] = {
                Platform::Tr("Logcat"),
                Platform::Tr("Shell"),
                Platform::Tr("Packages & Files"),
                Platform::Tr("Connections"),
            };
            {
                AdbDeviceSession &current = Adb::SelectedSession(workspace);
                ImGui::PushID(current.Serial.c_str());
                int tab = static_cast<int>(current.Tab);
                if (CategoryChipRow(tabs, IM_ARRAYSIZE(tabs), tab)) {
                    current.Tab = static_cast<AdbWorkspaceTab>(tab);
                }
                ImGui::PopID();
            }

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float right = ImGui::GetWindowContentRegionMax().x;
            const float tabsEnd = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
            const float room = std::max(1.0F, right - tabsEnd - spacing);
            const float chipH = ImGui::GetItemRectSize().y;
            ImGui::SameLine(0.0F, spacing);
            const float comboH = ImGui::GetFrameHeight();
            if (chipH > comboH) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((chipH - comboH) * 0.5F));
            }
            Adb::DevicePicker(context, snapshot, room);
            Adb::SyncPortRoutes(context, snapshot);
            const AdbDeviceSession &session = Adb::SelectedSession(workspace);
            const Core::AdbDevice *device = Adb::SelectedDevice(snapshot, workspace.SelectedSerial);
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float contentHeight = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - gap);
            const bool scroll = session.Tab == AdbWorkspaceTab::PackagesFiles || session.Tab == AdbWorkspaceTab::Connections;
            ImGui::PushID(session.Serial.c_str());
            ImGui::BeginChild(
                "##AdbTabBody",
                ImVec2(0.0F, contentHeight),
                ImGuiChildFlags_None,
                scroll ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoScrollbar
            );
            switch (session.Tab) {
                case AdbWorkspaceTab::Logcat:
                    Adb::BuildAdbLogcatTab(context, device);
                    break;
                case AdbWorkspaceTab::Shell:
                    Adb::BuildAdbShellTab(context, device);
                    break;
                case AdbWorkspaceTab::PackagesFiles:
                    Adb::BuildAdbPackagesFilesTab(context, device);
                    break;
                case AdbWorkspaceTab::Connections:
                    Adb::BuildAdbConnectionsTab(context, snapshot);
                    break;
            }
            ImGui::EndChild();
            std::optional<std::size_t> capturedLines;
            if (session.Tab == AdbWorkspaceTab::Logcat) {
                if (const auto log = context.Host.Adb.LogcatBuffer(session.Serial)) {
                    capturedLines = log->Size();
                }
            }
            ImGui::PopID();
            Adb::ShowTabStatus(workspace, session, snapshot, device, capturedLines);
        }

        void Confirmations(Context &context) {
            Adb::BuildUninstallPackageDialog(context);
            Adb::BuildAdbPackageListDialog(context);
            Adb::BuildClearDeviceLogDialog(context);
        }
    }

    void BuildAdbWorkspace(Context &context) {
        Adb::PollTask(context);
        if (context.Shell.Workspace != WorkspacePage::Adb) {
            return;
        }
        SetNativeWindowPage(context, Platform::Tr("Android Debug Bridge (ADB)"));

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            StyleVar sv;
            sv.Push(ImGuiStyleVar_WindowRounding, 0.0F);
            sv.Push(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::Begin("##AdbWorkspace", nullptr, WORKSPACE_WINDOW_FLAGS);
        }

        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##AdbBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        Core::AdbDeviceSnapshot snapshot;
        const bool hasAdb = !context.Host.Sdk.AdbPath.empty();
        if (!hasAdb) {
            context.Workspaces.Adb.ShowDevicesWindow = false;
            context.Workspaces.Adb.ShowWifiConnectHelp = false;
            if (NoticeCard(
                    Tone::Negative,
                    Platform::Tr("Missing Platform Tools (adb)"),
                    Platform::Tr("Install Google's official platform-tools to use devices, shell, transfers, and logcat."),
                    Platform::Tr("Install platform-tools...")
                )) {
                CloseAdbWorkspace(context);
                OpenPlatformToolsInstall(context);
            }
        } else {
            context.Host.Adb.StartTracking();
            snapshot = context.Host.Adb.Devices();
            for (Core::AdbDevice &device: snapshot.Devices) {
                if (device.EmulatorConsolePort > 0) {
                    device.AvdName = context.Host.Manager.GetAvdNameForConsolePort(device.EmulatorConsolePort);
                }
            }
            Adb::EnsureSelection(context, snapshot);
            Adb::EnsureDeviceProfiles(context, snapshot);
            WorkspaceBody(context, snapshot);
        }
        ImGui::EndChild();
        Confirmations(context);
        ImGui::End();
        if (hasAdb) {
            BuildAdbDevicesDialog(context, snapshot);
        }
    }

    void CloseAdbWorkspace(Context &context) {
        auto &workspace = context.Workspaces.Adb;
        for (const auto &entry: workspace.Sessions) {
            if (entry.second) {
                entry.second->Cancel.store(true);
            }
        }
        context.Shell.Workspace = WorkspacePage::Home;
        if (Adb::AnySessionBusy(workspace)) {
            workspace.CloseRequested = true;
            return;
        }
        Adb::LeaveAdb(context);
    }
}

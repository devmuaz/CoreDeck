//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_workspace.h"

#include "gui/adb/adb_controller.h"
#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_hooks_tab.h"
#include "gui/frida/frida_java_tab.h"
#include "gui/frida/frida_memory_tab.h"
#include "gui/frida/frida_modules_tab.h"
#include "gui/frida/frida_script_tab.h"
#include "gui/frida/frida_session_tab.h"
#include "gui/frida/frida_settings_tab.h"
#include "gui/frida/frida_widgets.h"
#include "gui/sdk/onboarding_screen.h"
#include "gui/widgets/device_picker.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void WorkspaceBody(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            auto &workspace = context.Workspaces.Frida;
            Frida::EnsureSelection(context, snapshot);
            FridaDeviceSession &session = Frida::SelectedSession(workspace);
            const char *tabs[] = {
                Platform::Tr("Session"),
                Platform::Tr("Script"),
                Platform::Tr("Hooks"),
                Platform::Tr("Java"),
                Platform::Tr("Modules"),
                Platform::Tr("Memory"),
                Platform::Tr("Settings"),
            };
            ImGui::PushID(session.Serial.c_str());
            int tab = static_cast<int>(session.Tab);
            if (CategoryChipRow(tabs, IM_ARRAYSIZE(tabs), tab)) {
                session.Tab = static_cast<FridaWorkspaceTab>(tab);
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
            DevicePicker("##FridaDevice", workspace.SelectedSerial, snapshot, room);
            const Core::AdbDevice *device = Adb::SelectedDevice(snapshot, workspace.SelectedSerial);
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float contentHeight = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - gap);
            ImGui::BeginChild("##FridaTabBody", ImVec2(0.0F, contentHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            switch (session.Tab) {
                case FridaWorkspaceTab::Session:
                    Frida::BuildFridaSessionTab(context, session, device);
                    break;
                case FridaWorkspaceTab::Script:
                    Frida::BuildFridaScriptTab(context, session);
                    break;
                case FridaWorkspaceTab::Hooks:
                    Frida::BuildFridaHooksTab(context, session);
                    break;
                case FridaWorkspaceTab::Java:
                    Frida::BuildFridaJavaTab(context, session);
                    break;
                case FridaWorkspaceTab::Modules:
                    Frida::BuildFridaModulesTab(context, session);
                    break;
                case FridaWorkspaceTab::Memory:
                    Frida::BuildFridaMemoryTab(context, session);
                    break;
                case FridaWorkspaceTab::Settings:
                    Frida::BuildFridaSettingsTab(context);
                    break;
            }
            ImGui::EndChild();
            ImGui::PopID();
            Frida::BuildFridaPrepareDialog(workspace);
            Frida::ShowFridaStatus(workspace, session, device);
        }
    }

    void BuildFridaWorkspace(Context &context) {
        Frida::PollFrida(context);
        if (context.Shell.Workspace != WorkspacePage::Frida) {
            return;
        }
        SetNativeWindowPage(context, Platform::Tr("Frida"));
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            StyleVar sv;
            sv.Push(ImGuiStyleVar_WindowRounding, 0.0F);
            sv.Push(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::Begin("##FridaWorkspace", nullptr, WORKSPACE_WINDOW_FLAGS);
        }
        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##FridaBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        if (context.Host.Sdk.AdbPath.empty()) {
            if (NoticeCard(
                    Tone::Negative,
                    Platform::Tr("Missing Platform Tools (adb)"),
                    Platform::Tr("Install Google's official platform-tools before starting Frida on a device."),
                    Platform::Tr("Install platform-tools...")
                )) {
                CloseFridaWorkspace(context);
                OpenPlatformToolsInstall(context);
            }
        } else {
            context.Host.Adb.StartTracking();
            context.Host.Frida.Start();
            Core::AdbDeviceSnapshot snapshot = context.Host.Adb.Devices();
            for (Core::AdbDevice &device: snapshot.Devices) {
                if (device.EmulatorConsolePort > 0) {
                    device.AvdName = context.Host.Manager.GetAvdNameForConsolePort(device.EmulatorConsolePort);
                }
            }
            WorkspaceBody(context, snapshot);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseFridaWorkspace(Context &context) {
        auto &workspace = context.Workspaces.Frida;
        for (const auto &entry: workspace.Sessions) {
            if (entry.second) {
                entry.second->Cancel.store(true);
            }
        }
        context.Shell.Workspace = WorkspacePage::Home;
        if (Frida::AnySessionWorking(workspace)) {
            workspace.CloseRequested = true;
            return;
        }
        for (const auto &entry: workspace.Sessions) {
            if (entry.second != nullptr && entry.second->Phase == FridaServerPhase::Ready) {
                workspace.CloseRequested = true;
                return;
            }
        }
    }
}

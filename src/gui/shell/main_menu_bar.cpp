//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"
#include <GLFW/glfw3.h>

#include <algorithm>

#include "gui/shell/main_menu_bar.h"
#include "gui/adb/adb_workspace.h"
#include "gui/apk/apk_analyzer_workspace.h"
#include "gui/apk/apk_signer_workspace.h"
#include "gui/apk/jar_signer_workspace.h"
#include "gui/apk/keytool_workspace.h"
#include "gui/sdk/health_check_dialog.h"
#include "gui/shell/application.h"
#include "gui/theme/theme.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        bool ToolWorkspaceOpen(const WorkspacePage page) {
            return page == WorkspacePage::Adb || page == WorkspacePage::ApkAnalyzer || page == WorkspacePage::ApkSigner || page == WorkspacePage::Keytool ||
                   page == WorkspacePage::JarSigner;
        }

        void CloseToolWorkspace(Context &context) {
            switch (context.Shell.Workspace) {
                case WorkspacePage::Adb:
                    CloseAdbWorkspace(context);
                    break;
                case WorkspacePage::ApkAnalyzer:
                    CloseApkAnalyzerWorkspace(context);
                    break;
                case WorkspacePage::ApkSigner:
                    CloseApkSignerWorkspace(context);
                    break;
                case WorkspacePage::Keytool:
                    CloseKeytoolWorkspace(context);
                    break;
                case WorkspacePage::JarSigner:
                    CloseJarSignerWorkspace(context);
                    break;
                default:
                    break;
            }
        }

        void WorkspaceClose(Context &context) {
            if (!ToolWorkspaceOpen(context.Shell.Workspace)) {
                return;
            }

            const std::string label = Platform::Tr("Close Workspace");
            const float dpi = GetDpiScale();
            const ImVec2 text = ImGui::CalcTextSize(label.c_str());
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const float inset = 5.0F * dpi;
            const float buttonH = std::max(text.y, windowSize.y - (inset * 2.0F));
            const float padY = std::max(0.0F, (buttonH - text.y) * 0.5F);
            const float padX = std::max(12.0F * dpi, padY + (4.0F * dpi));
            const float buttonW = text.x + (padX * 2.0F);
            const float marginX = 8.0F * dpi;
            ImVec2 pos(windowPos.x + windowSize.x - marginX - buttonW, windowPos.y + ((windowSize.y - buttonH) * 0.5F));
            const float minX = ImGui::GetCursorScreenPos().x + marginX;
            pos.x = std::max(pos.x, minX);

            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(pos);
            bool clicked = false;
            {
                StyleVar sv;
                sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
                sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
                sv.Push(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
                clicked = PrimaryButton(label.c_str(), true, ImVec2(buttonW, buttonH));
            }
            ImGui::SetCursorScreenPos(savedCursor);
            if (clicked) {
                CloseToolWorkspace(context);
            }
        }
    }

    void BuildMainMenuBar(Context &context) {
        const MenuStyle ms;

        if (ImGui::BeginMainMenuBar()) {
            if (RoundedBeginMenu(Platform::Tr("File"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::GEAR, Platform::Tr("Preferences...")).c_str())) {
                    OpenPreferencesDialog(context);
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::POWER_OFF, Platform::Tr("Quit")).c_str(), nullptr, false, context.Shell.MainWindow != nullptr)) {
                    glfwSetWindowShouldClose(context.Shell.MainWindow, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Platform::Tr("View"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::LIST, context.Shell.AvdListPanel ? Platform::Tr("Hide AVD List") : Platform::Tr("Show AVD List")).c_str())) {
                    context.Shell.AvdListPanel = !context.Shell.AvdListPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SLIDERS, context.Shell.OptionsPanel ? Platform::Tr("Hide Options") : Platform::Tr("Show Options")).c_str())) {
                    context.Shell.OptionsPanel = !context.Shell.OptionsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, context.Shell.DetailsPanel ? Platform::Tr("Hide Details") : Platform::Tr("Show Details")).c_str())) {
                    context.Shell.DetailsPanel = !context.Shell.DetailsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::TERMINAL, context.Shell.LogPanel ? Platform::Tr("Hide Output Log") : Platform::Tr("Show Output Log")).c_str())) {
                    context.Shell.LogPanel = !context.Shell.LogPanel;
                    PersistAppSettings(context);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Platform::Tr("Tools"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::HEART_PULSE, Platform::Tr("Health Check...")).c_str())) {
                    OpenHealthCheckDialog(context);
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, Platform::Tr("APK Analyzer")).c_str())) {
                    context.Shell.Workspace = WorkspacePage::ApkAnalyzer;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::TERMINAL, Platform::Tr("Android Debug Bridge (ADB)")).c_str())) {
                    context.Shell.Workspace = WorkspacePage::Adb;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SIGNATURE, Platform::Tr("APK Signer")).c_str())) {
                    context.Shell.Workspace = WorkspacePage::ApkSigner;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::KEY, Platform::Tr("Java Keytool")).c_str())) {
                    context.Shell.Workspace = WorkspacePage::Keytool;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SIGNATURE, Platform::Tr("Java Jar Signer")).c_str())) {
                    context.Shell.Workspace = WorkspacePage::JarSigner;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::HARD_DRIVE, Platform::Tr("Storage Overview")).c_str())) {
                    OpenStorageDialog(context);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Platform::Tr("Help"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::INFO, Platform::Tr("About CoreDeck")).c_str())) {
                    OpenAboutDialog(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::REFRESH, Platform::Tr("Check for Updates...")).c_str(), nullptr, false, !context.Dialogs.Update.UpdateCheckInFlight)) {
                    context.Dialogs.Update.RequestManualUpdateCheck = true;
                }
                ImGui::EndMenu();
            }

            WorkspaceClose(context);
            ImGui::EndMainMenuBar();
        }
    }
}

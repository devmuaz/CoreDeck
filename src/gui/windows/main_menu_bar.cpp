//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"
#include <GLFW/glfw3.h>

#include <algorithm>

#include "main_menu_bar.h"
#include "apk_analyzer.h"
#include "apk_signer.h"
#include "health_check.h"
#include "jar_signer.h"
#include "keytool.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"
#include "../../core/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        bool ToolWorkspaceOpen(const WorkspacePage page) {
            return page == WorkspacePage::ApkAnalyzer || page == WorkspacePage::ApkSigner || page == WorkspacePage::Keytool ||
                   page == WorkspacePage::JarSigner;
        }

        void CloseToolWorkspace(Context &context) {
            switch (context.UI.Workspace) {
                case WorkspacePage::ApkAnalyzer:
                    CloseApkAnalyzer(context);
                    break;
                case WorkspacePage::ApkSigner:
                    CloseApkSigner(context);
                    break;
                case WorkspacePage::Keytool:
                    CloseKeytool(context);
                    break;
                case WorkspacePage::JarSigner:
                    CloseJarSigner(context);
                    break;
                default:
                    break;
            }
        }

        void WorkspaceClose(Context &context) {
            if (!ToolWorkspaceOpen(context.UI.Workspace)) {
                return;
            }

            const std::string label = Core::Tr("Close Workspace");
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
            if (RoundedBeginMenu(Core::Tr("File"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::GEAR, Core::Tr("Preferences...")).c_str())) {
                    context.UI.ShowPreferences = true;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::POWER_OFF, Core::Tr("Quit")).c_str(), nullptr, false, context.UI.MainWindow != nullptr)) {
                    glfwSetWindowShouldClose(context.UI.MainWindow, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Core::Tr("View"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::LIST, context.UI.ShowAvdListPanel ? Core::Tr("Hide AVD List") : Core::Tr("Show AVD List")).c_str())) {
                    context.UI.ShowAvdListPanel = !context.UI.ShowAvdListPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SLIDERS, context.UI.ShowOptionsPanel ? Core::Tr("Hide Options") : Core::Tr("Show Options")).c_str())) {
                    context.UI.ShowOptionsPanel = !context.UI.ShowOptionsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, context.UI.ShowDetailsPanel ? Core::Tr("Hide Details") : Core::Tr("Show Details")).c_str())) {
                    context.UI.ShowDetailsPanel = !context.UI.ShowDetailsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::TERMINAL, context.UI.ShowLogPanel ? Core::Tr("Hide Output Log") : Core::Tr("Show Output Log")).c_str())) {
                    context.UI.ShowLogPanel = !context.UI.ShowLogPanel;
                    PersistAppSettings(context);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Core::Tr("Tools"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::HEART_PULSE, Core::Tr("Health Check...")).c_str())) {
                    OpenHealthCheckDialog(context);
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, Core::Tr("APK Analyzer")).c_str())) {
                    context.UI.Workspace = WorkspacePage::ApkAnalyzer;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SIGNATURE, Core::Tr("APK Signer")).c_str())) {
                    context.UI.Workspace = WorkspacePage::ApkSigner;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::KEY, Core::Tr("Java Keytool")).c_str())) {
                    context.UI.Workspace = WorkspacePage::Keytool;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SIGNATURE, Core::Tr("Java Jar Signer")).c_str())) {
                    context.UI.Workspace = WorkspacePage::JarSigner;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::HARD_DRIVE, Core::Tr("Storage Overview")).c_str())) {
                    context.UI.ShowStorageDialog = true;
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Core::Tr("Help"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::INFO, Core::Tr("About CoreDeck")).c_str())) {
                    context.UI.ShowAboutDialog = true;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::REFRESH, Core::Tr("Check for Updates...")).c_str(), nullptr, false, !context.Updates.UpdateCheckInFlight)) {
                    context.Updates.RequestManualUpdateCheck = true;
                }
                ImGui::EndMenu();
            }

            WorkspaceClose(context);
            ImGui::EndMainMenuBar();
        }
    }
}

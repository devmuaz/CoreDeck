//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"
#include <GLFW/glfw3.h>

#include "main_menu_bar.h"
#include "health_check.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    void BuildMainMenuBar(Context &context) {
        MenuStyle ms;

        if (ImGui::BeginMainMenuBar()) {
            if (RoundedBeginMenu(Tr("File"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::GEAR, Tr("Preferences...")).c_str())) {
                    context.UI.ShowPreferences = true;
                }
                ImGui::Separator();
                if (RoundedMenuItem(IconWithLabel(Icons::POWER_OFF, Tr("Quit")).c_str(), nullptr, false, context.UI.MainWindow != nullptr)) {
                    glfwSetWindowShouldClose(context.UI.MainWindow, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Tr("View"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::LIST, context.UI.ShowAvdListPanel ? Tr("Hide AVD List") : Tr("Show AVD List")).c_str())) {
                    context.UI.ShowAvdListPanel = !context.UI.ShowAvdListPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::SLIDERS, context.UI.ShowOptionsPanel ? Tr("Hide Options") : Tr("Show Options")).c_str())) {
                    context.UI.ShowOptionsPanel = !context.UI.ShowOptionsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, context.UI.ShowDetailsPanel ? Tr("Hide Details") : Tr("Show Details")).c_str())) {
                    context.UI.ShowDetailsPanel = !context.UI.ShowDetailsPanel;
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::TERMINAL, context.UI.ShowLogPanel ? Tr("Hide Output Log") : Tr("Show Output Log")).c_str())) {
                    context.UI.ShowLogPanel = !context.UI.ShowLogPanel;
                    PersistAppSettings(context);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Tr("Tools"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::HEART_PULSE, Tr("Health Check...")).c_str())) {
                    OpenHealthCheckDialog(context);
                }
                if (RoundedMenuItem(IconWithLabel(Icons::FILE_LINES, Tr("APK Analyzer")).c_str())) {
                    context.UI.ShowApkAnalyzerWindow = true;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::HARD_DRIVE, Tr("Storage Overview")).c_str())) {
                    context.UI.ShowStorageDialog = true;
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu(Tr("Help"))) {
                if (RoundedMenuItem(IconWithLabel(Icons::INFO, Tr("About CoreDeck")).c_str())) {
                    context.UI.ShowAboutDialog = true;
                }
                if (RoundedMenuItem(IconWithLabel(Icons::REFRESH, Tr("Check for Updates...")).c_str(), nullptr, false, !context.Updates.UpdateCheckInFlight)) {
                    context.Updates.RequestManualUpdateCheck = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}

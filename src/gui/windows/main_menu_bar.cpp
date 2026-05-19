//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"
#include <GLFW/glfw3.h>

#include "main_menu_bar.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"

namespace CoreDeck {
    void BuildMainMenuBar(Context &context) {
        MenuStyle ms;

        if (ImGui::BeginMainMenuBar()) {
            if (RoundedBeginMenu("File")) {
                if (RoundedMenuItem("Preferences...")) {
                    context.UI.ShowPreferences = true;
                }
                ImGui::Separator();
                if (RoundedMenuItem("Quit", nullptr, false, context.UI.MainWindow != nullptr)) {
                    glfwSetWindowShouldClose(context.UI.MainWindow, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu("View")) {
                if (RoundedMenuItem("Show AVD List Window", nullptr, &context.UI.ShowAvdListPanel)) {
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem("Show Options Window", nullptr, &context.UI.ShowOptionsPanel)) {
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem("Show Details Window", nullptr, &context.UI.ShowDetailsPanel)) {
                    PersistAppSettings(context);
                }
                if (RoundedMenuItem("Show Output Log Window", nullptr, &context.UI.ShowLogPanel)) {
                    PersistAppSettings(context);
                }
                ImGui::Separator();
                if (RoundedMenuItem("Storage Overview")) {
                    context.UI.ShowStorageDialog = true;
                }
                ImGui::EndMenu();
            }

            if (RoundedBeginMenu("Help")) {
                if (RoundedMenuItem(IconWithLabel(Icons::INFO, "About CoreDeck").c_str())) {
                    context.UI.ShowAboutDialog = true;
                }
                if (RoundedMenuItem("Check for Updates...", nullptr, false, !context.Updates.UpdateCheckInFlight)) {
                    context.Updates.RequestManualUpdateCheck = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}

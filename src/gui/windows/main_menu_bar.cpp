//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"
#include <GLFW/glfw3.h>

#include "main_menu_bar.h"
#include "../widgets.h"
#include "../icons.h"
#include "../application.h"

namespace CoreDeck {
    void BuildMainMenuBar(Context &context) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Preferences...", nullptr)) {
                    context.UI.ShowPreferences = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", nullptr, false, context.UI.MainWindow != nullptr)) {
                    glfwSetWindowShouldClose(context.UI.MainWindow, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("AVD List", nullptr, &context.UI.ShowAvdListPanel)) {
                    PersistAppSettings(context);
                }
                if (ImGui::MenuItem("Options", nullptr, &context.UI.ShowOptionsPanel)) {
                    PersistAppSettings(context);
                }
                if (ImGui::MenuItem("Details", nullptr, &context.UI.ShowDetailsPanel)) {
                    PersistAppSettings(context);
                }
                if (ImGui::MenuItem("Output Log", nullptr, &context.UI.ShowLogPanel)) {
                    PersistAppSettings(context);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Storage Overview...")) {
                    context.UI.ShowStorageDialog = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem(IconWithLabel(Icons::Info, "About CoreDeck").c_str())) {
                    context.UI.ShowAboutDialog = true;
                }
                if (ImGui::MenuItem("Check for Updates...", nullptr, false, !context.Updates.UpdateCheckInFlight)) {
                    context.Updates.RequestManualUpdateCheck = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}

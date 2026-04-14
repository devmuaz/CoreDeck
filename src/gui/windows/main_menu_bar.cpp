//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "main_menu_bar.h"
#include "../widgets.h"
#include "../theme.h"
#include "../application.h"

namespace CoreDeck {
    void BuildMainMenuBar(Context &context) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem(IconWithLabel(Icons::Info, "About CoreDeck").c_str())) {
                    context.ShowAboutDialog = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
}

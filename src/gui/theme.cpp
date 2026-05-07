//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "theme.h"

namespace CoreDeck {
    void ApplyCustomImGuiTheme() {
        auto &style = ImGui::GetStyle();

        style.WindowRounding = 6.0f;
        style.FrameRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.PopupRounding = 4.0f;
        style.FramePadding = ImVec2(8.0f, 8.0f);
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.ScrollbarSize = 10.0f;
        style.FrameBorderSize = 0.6f;
        style.TabRounding = 0.0f;
        style.TabBarBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;
        style.TabBarOverlineSize = 0.0f;

        auto &c = style.Colors;

        // Dock tabs — inactive
        c[ImGuiCol_Tab] = HexColor(Colors::Shadow, 0.0f);
        c[ImGuiCol_TabHovered] = HexColor(Colors::Shadow, 0.0f);
        c[ImGuiCol_TabSelected] = HexColor(Colors::Shadow, 0.0f);
        c[ImGuiCol_TabSelectedOverline] = HexColor(Colors::Shadow, 0.0f);

        // Dock tabs — unfocused window
        c[ImGuiCol_TabDimmed] = HexColor(Colors::Shadow, 0.0f);
        c[ImGuiCol_TabDimmedSelected] = HexColor(Colors::Shadow, 0.0f);
        c[ImGuiCol_TabDimmedSelectedOverline] = HexColor(Colors::Shadow, 0.0f);

        // Docking preview overlay
        c[ImGuiCol_DockingPreview] = HexColor(Colors::TextPrimary, 0.20f);
        c[ImGuiCol_DockingEmptyBg] = HexColor(Colors::Surface0);

        // Window
        c[ImGuiCol_WindowBg] = HexColor(Colors::Surface0);
        c[ImGuiCol_ChildBg] = HexColor(Colors::Surface0);
        c[ImGuiCol_PopupBg] = HexColor(Colors::Surface1, 0.98f);
        c[ImGuiCol_ModalWindowDimBg] = HexColor(Colors::Shadow, 0.55f);

        // Borders
        c[ImGuiCol_Border] = HexColor(Colors::Surface4);
        c[ImGuiCol_BorderShadow] = HexColor(Colors::Shadow, 0.0f);

        // Text
        c[ImGuiCol_Text] = HexColor(Colors::TextPrimary);
        c[ImGuiCol_TextDisabled] = HexColor(Colors::TextMuted);

        // Headers
        c[ImGuiCol_Header] = HexColor(Colors::Surface3);
        c[ImGuiCol_HeaderHovered] = HexColor(Colors::Surface3);
        c[ImGuiCol_HeaderActive] = HexColor(Colors::Surface4);

        // Buttons
        c[ImGuiCol_Button] = HexColor(Colors::Surface2);
        c[ImGuiCol_ButtonHovered] = HexColor(Colors::Surface4);
        c[ImGuiCol_ButtonActive] = HexColor(Colors::Surface0);

        // Frame
        c[ImGuiCol_FrameBg] = HexColor(Colors::Surface1);
        c[ImGuiCol_FrameBgHovered] = HexColor(Colors::Surface3);
        c[ImGuiCol_FrameBgActive] = HexColor(Colors::Surface3);

        // Checkbox
        c[ImGuiCol_CheckMark] = HexColor(Colors::TextPrimary);

        // Slider
        c[ImGuiCol_SliderGrab] = HexColor(Colors::TextPrimary);
        c[ImGuiCol_SliderGrabActive] = HexColor(Colors::TextOnDark);

        // Scrollbar
        c[ImGuiCol_ScrollbarBg] = HexColor(Colors::Surface0);
        c[ImGuiCol_ScrollbarGrab] = HexColor(Colors::Surface4);
        c[ImGuiCol_ScrollbarGrabHovered] = HexColor(Colors::Border);
        c[ImGuiCol_ScrollbarGrabActive] = HexColor(Colors::BorderHover);

        // Separator
        c[ImGuiCol_Separator] = HexColor(Colors::Surface2);
        c[ImGuiCol_SeparatorHovered] = HexColor(Colors::BorderStrong);
        c[ImGuiCol_SeparatorActive] = HexColor(Colors::TextMuted);

        // Menu bar
        c[ImGuiCol_MenuBarBg] = HexColor(Colors::Surface0);

        // Title bar
        c[ImGuiCol_TitleBg] = HexColor(Colors::Surface0);
        c[ImGuiCol_TitleBgActive] = HexColor(Colors::Surface1);
        c[ImGuiCol_TitleBgCollapsed] = HexColor(Colors::Surface0);

        // Text selection
        c[ImGuiCol_TextSelectedBg] = HexColor(Colors::BorderSubtle, 0.60f);

        // Resize grip
        c[ImGuiCol_ResizeGrip] = HexColor(Colors::Surface4, 0.25f);
        c[ImGuiCol_ResizeGripHovered] = HexColor(Colors::BorderStrong, 0.65f);
        c[ImGuiCol_ResizeGripActive] = HexColor(Colors::TextMuted, 0.95f);
    }
}
//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#include "imgui.h"

#include "theme.h"

namespace CoreDeck {
    void ApplyCustomImGuiTheme() {
        auto &style = ImGui::GetStyle();

        style.WindowRounding = 6.0F;
        style.FrameRounding = 6.0F;
        style.GrabRounding = 6.0F;
        style.ScrollbarRounding = 6.0F;
        style.PopupRounding = 4.0F;
        style.FramePadding = ImVec2(8.0F, 8.0F);
        style.ItemSpacing = ImVec2(8.0F, 8.0F);
        style.WindowPadding = ImVec2(8.0F, 8.0F);
        style.ScrollbarSize = 10.0F;
        style.FrameBorderSize = 0.6F;
        style.TabRounding = 0.0F;
        style.TabBarBorderSize = 0.0F;
        style.TabBorderSize = 0.0F;
        style.TabBarOverlineSize = 0.0F;

        auto &c = style.Colors;

        // Dock tabs — inactive
        c[ImGuiCol_Tab] = HexColor(Colors::SHADOW, 0.0F);
        c[ImGuiCol_TabHovered] = HexColor(Colors::SHADOW, 0.0F);
        c[ImGuiCol_TabSelected] = HexColor(Colors::SHADOW, 0.0F);
        c[ImGuiCol_TabSelectedOverline] = HexColor(Colors::SHADOW, 0.0F);

        // Dock tabs — unfocused window
        c[ImGuiCol_TabDimmed] = HexColor(Colors::SHADOW, 0.0F);
        c[ImGuiCol_TabDimmedSelected] = HexColor(Colors::SHADOW, 0.0F);
        c[ImGuiCol_TabDimmedSelectedOverline] = HexColor(Colors::SHADOW, 0.0F);

        // Docking preview overlay
        c[ImGuiCol_DockingPreview] = HexColor(Colors::TEXT_PRIMARY, 0.20F);
        c[ImGuiCol_DockingEmptyBg] = HexColor(Colors::SURFACE0);

        // Window
        c[ImGuiCol_WindowBg] = HexColor(Colors::SURFACE0);
        c[ImGuiCol_ChildBg] = HexColor(Colors::SURFACE0);
        c[ImGuiCol_PopupBg] = HexColor(Colors::SURFACE1, 0.98F);
        c[ImGuiCol_ModalWindowDimBg] = HexColor(Colors::SHADOW, 0.55F);

        // Borders
        c[ImGuiCol_Border] = HexColor(Colors::SURFACE4);
        c[ImGuiCol_BorderShadow] = HexColor(Colors::SHADOW, 0.0F);

        // Text
        c[ImGuiCol_Text] = HexColor(Colors::TEXT_PRIMARY);
        c[ImGuiCol_TextDisabled] = HexColor(Colors::TEXT_MUTED);

        // Headers
        c[ImGuiCol_Header] = HexColor(Colors::SURFACE3);
        c[ImGuiCol_HeaderHovered] = HexColor(Colors::SURFACE3);
        c[ImGuiCol_HeaderActive] = HexColor(Colors::SURFACE4);

        // Buttons
        c[ImGuiCol_Button] = HexColor(Colors::SURFACE2);
        c[ImGuiCol_ButtonHovered] = HexColor(Colors::SURFACE4);
        c[ImGuiCol_ButtonActive] = HexColor(Colors::SURFACE0);

        // Frame
        c[ImGuiCol_FrameBg] = HexColor(Colors::SURFACE1);
        c[ImGuiCol_FrameBgHovered] = HexColor(Colors::SURFACE3);
        c[ImGuiCol_FrameBgActive] = HexColor(Colors::SURFACE3);

        // Checkbox
        c[ImGuiCol_CheckMark] = HexColor(Colors::TEXT_PRIMARY);

        // Slider
        c[ImGuiCol_SliderGrab] = HexColor(Colors::TEXT_PRIMARY);
        c[ImGuiCol_SliderGrabActive] = HexColor(Colors::TEXT_ON_DARK);

        // Scrollbar
        c[ImGuiCol_ScrollbarBg] = HexColor(Colors::SURFACE0);
        c[ImGuiCol_ScrollbarGrab] = HexColor(Colors::SURFACE4);
        c[ImGuiCol_ScrollbarGrabHovered] = HexColor(Colors::BORDER);
        c[ImGuiCol_ScrollbarGrabActive] = HexColor(Colors::BORDER_HOVER);

        // Separator
        c[ImGuiCol_Separator] = HexColor(Colors::SURFACE2);
        c[ImGuiCol_SeparatorHovered] = HexColor(Colors::BORDER_STRONG);
        c[ImGuiCol_SeparatorActive] = HexColor(Colors::TEXT_MUTED);

        // Menu bar
        c[ImGuiCol_MenuBarBg] = HexColor(Colors::SURFACE0);

        // Title bar
        c[ImGuiCol_TitleBg] = HexColor(Colors::SURFACE0);
        c[ImGuiCol_TitleBgActive] = HexColor(Colors::SURFACE1);
        c[ImGuiCol_TitleBgCollapsed] = HexColor(Colors::SURFACE0);

        // Text selection
        c[ImGuiCol_TextSelectedBg] = HexColor(Colors::BORDER_SUBTLE, 0.60F);

        // Resize grip
        c[ImGuiCol_ResizeGrip] = HexColor(Colors::SURFACE4, 0.25F);
        c[ImGuiCol_ResizeGripHovered] = HexColor(Colors::BORDER_STRONG, 0.65F);
        c[ImGuiCol_ResizeGripActive] = HexColor(Colors::TEXT_MUTED, 0.95F);
    }
}
//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include "utilities.h"

#include "imgui.h"

namespace Emu {
    void ApplyCustomImGuiTheme() {
        auto &style = ImGui::GetStyle();

        style.WindowRounding = 0.0f;
        style.FrameRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
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
        c[ImGuiCol_Tab] = HexColor("#000000", 0.0f);
        c[ImGuiCol_TabHovered] = HexColor("#000000", 0.0f);
        c[ImGuiCol_TabSelected] = HexColor("#000000", 0.0f);
        c[ImGuiCol_TabSelectedOverline] = HexColor("#000000", 0.0f);

        // Dock tabs — unfocused window
        c[ImGuiCol_TabDimmed] = HexColor("#000000", 0.0f);
        c[ImGuiCol_TabDimmedSelected] = HexColor("#000000", 0.0f);
        c[ImGuiCol_TabDimmedSelectedOverline] = HexColor("#000000", 0.0f);

        // Docking preview overlay
        c[ImGuiCol_DockingPreview] = HexColor("#F2F2F2", 0.20f);
        c[ImGuiCol_DockingEmptyBg] = HexColor("#0A0A0C");

        // Window
        c[ImGuiCol_WindowBg] = HexColor("#0F0F12");
        c[ImGuiCol_ChildBg] = HexColor("#0F0F12");
        c[ImGuiCol_PopupBg] = HexColor("#141417", 0.98f);

        // Borders
        c[ImGuiCol_Border] = HexColor("#2E2E33");
        c[ImGuiCol_BorderShadow] = HexColor("#000000", 0.0f);

        // Text
        c[ImGuiCol_Text] = HexColor("#F2F2F2");
        c[ImGuiCol_TextDisabled] = HexColor("#66666B");

        // Headers
        c[ImGuiCol_Header] = HexColor("#1F1F21");
        c[ImGuiCol_HeaderHovered] = HexColor("#29292B");
        c[ImGuiCol_HeaderActive] = HexColor("#333336");

        // Buttons
        c[ImGuiCol_Button] = HexColor("#1A1A1C");
        c[ImGuiCol_ButtonHovered] = HexColor("#2E2E30");
        c[ImGuiCol_ButtonActive] = HexColor("#0F0F12");

        // Frame
        c[ImGuiCol_FrameBg] = HexColor("#141417");
        c[ImGuiCol_FrameBgHovered] = HexColor("#1F1F21");
        c[ImGuiCol_FrameBgActive] = HexColor("#242426");

        // Checkbox
        c[ImGuiCol_CheckMark] = HexColor("#F2F2F2");

        // Slider
        c[ImGuiCol_SliderGrab] = HexColor("#F2F2F2");
        c[ImGuiCol_SliderGrabActive] = HexColor("#CCCCCC");

        // Scrollbar
        c[ImGuiCol_ScrollbarBg] = HexColor("#0F0F12");
        c[ImGuiCol_ScrollbarGrab] = HexColor("#333336");
        c[ImGuiCol_ScrollbarGrabHovered] = HexColor("#47474A");
        c[ImGuiCol_ScrollbarGrabActive] = HexColor("#5C5C5E");

        // Separator
        c[ImGuiCol_Separator] = HexColor("#2E2E33");
        c[ImGuiCol_SeparatorHovered] = HexColor("#4D4D4F");
        c[ImGuiCol_SeparatorActive] = HexColor("#666669");

        // Title bar
        c[ImGuiCol_TitleBg] = HexColor("#0F0F12");
        c[ImGuiCol_TitleBgActive] = HexColor("#141417");
        c[ImGuiCol_TitleBgCollapsed] = HexColor("#0F0F12");

        // Text selection
        c[ImGuiCol_TextSelectedBg] = HexColor("#3F3F42", 0.60f);

        // Resize grip
        c[ImGuiCol_ResizeGrip] = HexColor("#333336", 0.25f);
        c[ImGuiCol_ResizeGripHovered] = HexColor("#4D4D4F", 0.65f);
        c[ImGuiCol_ResizeGripActive] = HexColor("#666669", 0.95f);
    }
}

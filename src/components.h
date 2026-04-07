//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_COMPONENTS_H
#define EMU_LAUNCHER_COMPONENTS_H
#include "imgui.h"

namespace Emu {
    struct StyleColor {
        int count = 0;

        void push(const ImGuiCol idx, const ImVec4 &color) {
            ImGui::PushStyleColor(idx, color);
            count++;
        }

        ~StyleColor() {
            if (count > 0) ImGui::PopStyleColor(count);
        }
    };

    struct StyleVar {
        int count = 0;

        void push(const ImGuiStyleVar idx, const float val) {
            ImGui::PushStyleVar(idx, val);
            count++;
        }

        void push(const ImGuiStyleVar idx, const ImVec2 &val) {
            ImGui::PushStyleVar(idx, val);
            count++;
        }

        ~StyleVar() {
            if (count > 0) ImGui::PopStyleVar(count);
        }
    };

    bool PrimaryButton(const char *label, bool isEnabled = true);

    bool NegativeButton(const char *label, bool isEnabled = true);

    bool PositiveButton(const char *label, bool isEnabled = true);

    bool WarningButton(const char *label, bool isEnabled = true);

    void StatusBadge(const char *label, bool isActive);

    bool SelectableItem(
        const char *label,
        bool isSelected,
        const char *rightText = nullptr,
        const ImVec4 &rightColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    void PropertyText(const char *label, const char *value);

    void PropertyTextWrapped(const char *label, const char *value);
}

#endif //EMU_LAUNCHER_COMPONENTS_H

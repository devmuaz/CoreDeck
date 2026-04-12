//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_COMPONENTS_H
#define EMU_LAUNCHER_COMPONENTS_H
#include "imgui.h"

namespace CoreDeck {
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

    enum class DialogResult { None, Confirmed, Cancelled };

    enum class DialogType { Default, Positive, Negative };

    struct DialogData {
        const char *Id{};
        bool &isOpen;
        const char *title{};
        const char *message{};
        const char *confirmButtonTitle{};
        const char *cancelButtonTitle{};
        DialogType type = DialogType::Default;
    };

    bool PrimaryButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool NegativeButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool PositiveButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool WarningButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    void StatusBadge(const char *label, bool isActive);

    bool SelectableItem(
        const char *label,
        bool isSelected,
        const char *rightText = nullptr,
        const ImVec4 &rightColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    bool PropertyText(const char *label, const char *value, bool isClickable = false);

    void PropertyTextWrapped(const char *label, const char *value);

    bool CollapsingHeader(const char *label, ImGuiTreeNodeFlags flags = 0);

    bool MenuButton(const char *label);

    bool MenuPopupItem(const char *label);

    DialogResult CustomDialog(const DialogData &data);
}

#endif //EMU_LAUNCHER_COMPONENTS_H

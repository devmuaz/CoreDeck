//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_COMPONENTS_H
#define EMU_LAUNCHER_COMPONENTS_H

#include <string>

#include "imgui.h"
#include "../core/utilities.h"

namespace CoreDeck {
    class StyleColor {
    public:
        StyleColor() = default;
        StyleColor(StyleColor &&) = delete;
        StyleColor &operator=(StyleColor &&) = delete;
        StyleColor(const StyleColor &) = delete;
        StyleColor &operator=(const StyleColor &) = delete;

        void Push(const ImGuiCol idx, const ImVec4 &color) {
            ImGui::PushStyleColor(idx, color);
            m_Count++;
        }

        ~StyleColor() {
            if (m_Count > 0) {
                ImGui::PopStyleColor(m_Count);
            }
        }

    private:
        int m_Count = 0;
    };

    class StyleVar {
    public:
        StyleVar() = default;
        StyleVar(StyleVar &&) = delete;
        StyleVar &operator=(StyleVar &&) = delete;
        StyleVar(const StyleVar &) = delete;
        StyleVar &operator=(const StyleVar &) = delete;

        void Push(const ImGuiStyleVar idx, const float val) {
            ImGui::PushStyleVar(idx, val);
            m_Count++;
        }

        void Push(const ImGuiStyleVar idx, const ImVec2 &val) {
            ImGui::PushStyleVar(idx, val);
            m_Count++;
        }

        ~StyleVar() {
            if (m_Count > 0) {
                ImGui::PopStyleVar(m_Count);
            }
        }

    private:
        int m_Count = 0;
    };

    struct LabeledIconStyle {
        const char *Icon;
        const char *Label;
        const char *Color;
    };

    struct PickerTableStyle {
        StyleColor Colors;
        StyleVar Vars;

        PickerTableStyle();
    };

    constexpr ImGuiTableFlags PICKER_TABLE_FLAGS =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    constexpr ImGuiWindowFlags WINDOW_NO_RESIZE_FLAGS =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking;

    constexpr ImGuiWindowFlags WINDOW_AUTO_RESIZE_FLAGS =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoDocking;

    enum class DialogResult : uint8_t {
        None,
        Confirmed,
        Cancelled
    };

    enum class DialogType : uint8_t {
        Default,
        Positive,
        Negative
    };

    struct DialogData {
        const char *Id{};
        bool &IsOpen; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        const char *Title{};
        const char *Message{};
        const char *ConfirmButtonTitle{};
        const char *CancelButtonTitle{};
        const char *BusyButtonTitle{};
        DialogType Type = DialogType::Default;
        bool IsBusy = false;
    };

    inline std::string IconWithLabel(const char *icon, const char *label) {
        return StrConcat(icon, " ", label);
    }

    bool PrimaryButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool NegativeButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool PositiveButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool WarningButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool PickerButton(const char *label, bool isEnabled = true, ImVec2 size = ImVec2(0, 0));

    bool ToggleButton(const char *label, bool &isToggled, ImVec2 size = ImVec2(0, 0));

    void StatusBadge(const char *label, bool isActive);

    bool SelectableItem(
        const char *label,
        bool isSelected,
        const char *rightText = nullptr,
        const ImVec4 &rightColor = ImVec4(1.0F, 1.0F, 1.0F, 1.0F),
        const char *leftIcon = nullptr,
        const ImVec4 &leftIconColor = ImVec4(1.0F, 1.0F, 1.0F, 1.0F)
    );

    bool PropertyText(const char *label, const char *value, bool isClickable = false, bool hasSpaceBetween = false);

    void PropertyTextWrapped(const char *label, const char *value, bool invertColors = false);

    bool CategoryChip(const char *label, bool isSelected);

    bool CollapsingHeader(const char *label, ImGuiTreeNodeFlags flags = 0);

    bool MenuButton(const char *label);

    bool MenuPopupItem(const char *label);

    DialogResult SimpleDialog(const DialogData &data);
}

#endif // EMU_LAUNCHER_COMPONENTS_H

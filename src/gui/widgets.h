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
        StyleColor(const StyleColor &) = delete;
        StyleColor(StyleColor &&) = delete;
        StyleColor &operator=(const StyleColor &) = delete;
        StyleColor &operator=(StyleColor &&) = delete;
        ~StyleColor();

        void Push(ImGuiCol idx, const ImVec4 &color);

    private:
        int m_Count = 0;
    };

    class StyleVar {
    public:
        StyleVar() = default;
        StyleVar(const StyleVar &) = delete;
        StyleVar(StyleVar &&) = delete;
        StyleVar &operator=(const StyleVar &) = delete;
        StyleVar &operator=(StyleVar &&) = delete;
        ~StyleVar();

        void Push(ImGuiStyleVar idx, float val);

        void Push(ImGuiStyleVar idx, const ImVec2 &val);

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

    inline float Em(const float n = 1.0F) {
        return ImGui::CalcTextSize("M").x * n;
    }

    inline float Eh(const float n = 1.0F) {
        return ImGui::GetTextLineHeightWithSpacing() * n;
    }

    inline ImVec2 EmV(const float w, const float h) {
        return ImVec2(Em(w), Eh(h));
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

    struct MenuStyle {
        StyleVar Vars;

        MenuStyle();
    };

    bool RoundedMenuItem(const char *label, const char *shortcut = nullptr, bool isSelected = false, bool isEnabled = true);

    bool RoundedMenuItem(const char *label, const char *shortcut, bool *pIsSelected, bool isEnabled = true);

    bool RoundedBeginMenu(const char *label, bool isEnabled = true);

    struct ComboStyle {
        StyleVar Vars;

        ComboStyle();
    };

    bool RoundedSelectable(const char *label, bool isSelected = false, ImGuiSelectableFlags flags = 0, const ImVec2 &size = ImVec2(0, 0));

    bool RoundedBeginPopupModal(const char *name, bool *pOpen = nullptr, ImGuiWindowFlags flags = 0);

    DialogResult SimpleDialog(const DialogData &data);

    bool SubtitledCheckbox(const char *id, bool *value, const char *label, const char *subtitle = nullptr, const char *tooltip = nullptr, float boxSize = 28.0F);
}

#endif // EMU_LAUNCHER_COMPONENTS_H

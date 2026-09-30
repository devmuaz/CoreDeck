//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <cstring>
#include <numbers>

#include "imgui.h"
#include "imgui_internal.h"

#include "widgets.h"
#include "utilities.h"
#include "theme.h"
#include "../core/file_dialog.h"
#include "../core/i18n.h"

namespace CoreDeck {

    void StyleColor::Push(ImGuiCol idx, const ImVec4 &color) {
        ImGui::PushStyleColor(idx, color);
        m_Count++;
    }

    StyleColor::~StyleColor() {
        if (m_Count > 0) {
            ImGui::PopStyleColor(m_Count);
        }
    }

    void StyleVar::Push(ImGuiStyleVar idx, float val) {
        ImGui::PushStyleVar(idx, val);
        m_Count++;
    }

    void StyleVar::Push(ImGuiStyleVar idx, const ImVec2 &val) {
        ImGui::PushStyleVar(idx, val);
        m_Count++;
    }

    StyleVar::~StyleVar() {
        if (m_Count > 0) {
            ImGui::PopStyleVar(m_Count);
        }
    }

    PickerTableStyle::PickerTableStyle() {
        Colors.Push(ImGuiCol_ChildBg, HexColor(Colors::SURFACE1));
        Colors.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
        Colors.Push(ImGuiCol_TableHeaderBg, HexColor(Colors::SURFACE2));
        Colors.Push(ImGuiCol_TableRowBg, HexColor(Colors::SHADOW, 0.0F));
        Colors.Push(ImGuiCol_TableRowBgAlt, HexColor(Colors::SURFACE2, 0.28F));
        Colors.Push(ImGuiCol_TableBorderLight, HexColor(Colors::SURFACE3));
        Colors.Push(ImGuiCol_TableBorderStrong, HexColor(Colors::SURFACE4));
        Colors.Push(ImGuiCol_Header, HexColor(Colors::SURFACE3, 0.65F));
        Colors.Push(ImGuiCol_HeaderHovered, HexColor(Colors::SURFACE4, 0.85F));
        Colors.Push(ImGuiCol_HeaderActive, HexColor(Colors::BORDER_SUBTLE));

        Vars.Push(ImGuiStyleVar_ChildRounding, 6.0F);
        Vars.Push(ImGuiStyleVar_ChildBorderSize, 1.0F);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(1.0F, 1.0F));
        Vars.Push(ImGuiStyleVar_CellPadding, ImVec2(8.0F, 8.0F));
    }

    namespace {
        bool StyledButton(const char *label, bool isEnabled, ImVec2 size, bool hasSpinner);
    }

    bool PrimaryButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
        if (IsLightColorScheme()) {
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE4));
        } else {
            sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE4, 0.6F));
            sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE0));
        }
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        sc.Push(ImGuiCol_Border, HexColor(Colors::BORDER_STRONG));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool NegativeButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::NEGATIVE_STRONG, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::NEGATIVE_STRONG, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::NEGATIVE_STRONG, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::NEGATIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::NEGATIVE));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool WarningButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::WARNING, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::WARNING, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::WARNING, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::WARNING_STRONG));
        sc.Push(ImGuiCol_Border, HexColor(Colors::WARNING_STRONG));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool PositiveButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleColor sc;
        sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.10F));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::POSITIVE_FILL, 0.20F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::POSITIVE_FILL, 0.30F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
        sc.Push(ImGuiCol_Border, HexColor(Colors::POSITIVE));

        return StyledButton(label, isEnabled, size, hasSpinner);
    }

    bool PickerButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
        StyleVar sv;
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0F, 0.5F));
        return PrimaryButton(label, isEnabled, size, hasSpinner);
    }

    bool ToggleButton(const char *label, bool &isToggled, const ImVec2 size) {
        StyleColor sc;
        if (isToggled) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::TEXT_PRIMARY, 0.10F));
            sc.Push(ImGuiCol_Border, HexColor(Colors::TEXT_PRIMARY, 0.75F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        }
        const bool clicked = ImGui::Button(label, size);
        if (clicked) {
            isToggled = !isToggled;
        }
        return clicked;
    }

    void StatusBadge(const char *label, const bool isActive) {
        StyleColor sc;
        StyleVar sv;

        if (isActive) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::POSITIVE_FILL, 0.10F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::POSITIVE));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::NEGATIVE_STRONG, 0.10F));
            sc.Push(ImGuiCol_Text, HexColor(Colors::NEGATIVE));
        }
        sc.Push(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_Button]);
        sc.Push(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_Button]);

        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
        sv.Push(ImGuiStyleVar_FrameRounding, 6.0F);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(6.0F, 2.0F));

        ImGui::Button(label);
    }

    void StatusMessage(const StatusMessageTone tone, const char *message) {
        if (message == nullptr || message[0] == '\0') {
            return;
        }

        const char *icon = Icons::INFO;
        const char *color = Colors::ACCENT_INFO;
        switch (tone) {
            case StatusMessageTone::Positive:
                icon = Icons::CHECK_CIRCLE;
                color = Colors::POSITIVE;
                break;
            case StatusMessageTone::Error:
                icon = Icons::TIMES_CIRCLE;
                color = Colors::NEGATIVE;
                break;
            case StatusMessageTone::Warning:
                icon = Icons::WARNING_TRIANGLE;
                color = Colors::WARNING;
                break;
            case StatusMessageTone::Info:
                break;
        }

        const float dpi = GetDpiScale();
        const float padX = 8.0F * dpi;
        const float padY = 5.0F * dpi;
        const float gap = 6.0F * dpi;
        const float rounding = 5.5F * dpi;
        float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->DC.TextWrapPos > 0.0F) {
            const float wrapWidth = ImGui::CalcWrapWidthForPos(ImGui::GetCursorScreenPos(), window->DC.TextWrapPos);
            avail = std::min(avail, std::max(1.0F, wrapWidth));
        }
        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        const float textWrap = std::max(1.0F, avail - (padX * 2.0F) - iconSize.x - gap);
        const ImVec2 textSize = ImGui::CalcTextSize(message, nullptr, false, textWrap);
        const float height = std::max(iconSize.y, textSize.y) + (padY * 2.0F);
        const float width = std::min(avail, (padX * 2.0F) + iconSize.x + gap + textSize.x);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 max(origin.x + width, origin.y + height);
        const ImU32 ink = ImGui::GetColorU32(HexColor(color));
        drawList->AddRectFilled(origin, max, ImGui::GetColorU32(HexColor(color, 0.08F)), rounding);
        drawList->AddRect(origin, max, ImGui::GetColorU32(HexColor(color, 0.35F)), rounding, 0, 1.0F * dpi);

        const float textY = origin.y + ((height - textSize.y) * 0.5F);
        const float iconY = origin.y + ((height - iconSize.y) * 0.5F);
        drawList->AddText(ImVec2(origin.x + padX, iconY), ink, icon);
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(origin.x + padX + iconSize.x + gap, textY),
            ink,
            message,
            nullptr,
            textWrap
        );
    }

    namespace {
        ImVec2 PointOnCircle(const ImVec2 &center, const float radius, const float angle) {
            return {center.x + (ImCos(angle) * radius), center.y + (ImSin(angle) * radius)};
        }

        void PaintSpinner(ImDrawList *draw, const ImVec2 &center, const float radius, const float thickness, const ImU32 color) {
            RequestSpinnerFrame();
            const auto time = static_cast<float>(ImGui::GetTime());
            constexpr float K_TURN = std::numbers::pi_v<float> * 2.0F;
            constexpr float K_SWEEP = std::numbers::pi_v<float> * 1.15F;
            const float start = time * (K_TURN * 0.8F);
            const float end = start + K_SWEEP;

            ImVec4 trackColor = ImGui::ColorConvertU32ToFloat4(color);
            trackColor.w *= 0.28F;
            constexpr int K_TRACK_SEGMENTS = 32;
            const float trackEnd = K_TURN * (static_cast<float>(K_TRACK_SEGMENTS - 1) / static_cast<float>(K_TRACK_SEGMENTS));
            draw->PathArcTo(center, radius, 0.0F, trackEnd, K_TRACK_SEGMENTS - 1);
            draw->PathStroke(ImGui::GetColorU32(trackColor), thickness, ImDrawFlags_Closed);

            draw->PathArcTo(center, radius, start, end, 24);
            draw->PathStroke(color, thickness);
            const float cap = thickness * 0.5F;
            draw->AddCircleFilled(PointOnCircle(center, radius, start), cap, color);
            draw->AddCircleFilled(PointOnCircle(center, radius, end), cap, color);
        }

        bool StyledButton(const char *label, const bool isEnabled, const ImVec2 size, const bool hasSpinner) {
            const char *text = label != nullptr ? label : "";
            const bool dimmed = !isEnabled && !hasSpinner;
            if (dimmed) {
                ImGui::BeginDisabled();
            }

            bool clicked = false;
            if (!hasSpinner) {
                clicked = ImGui::Button(text, size);
            } else {
                const ImGuiStyle &style = ImGui::GetStyle();
                const float diameter = ImGui::GetTextLineHeight();
                const float gap = style.ItemInnerSpacing.x;
                const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, true);
                const float contentWidth = diameter + gap + textSize.x;
                ImVec2 buttonSize = size;
                if (buttonSize.x <= 0.0F) {
                    buttonSize.x = contentWidth + (style.FramePadding.x * 2.0F);
                }

                ImGui::PushID(text);
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                ImGui::Button("##busy", buttonSize);
                ImGui::PopItemFlag();
                ImGui::PopItemFlag();
                ImGui::PopID();

                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                const float innerWidth = std::max(0.0F, (max.x - min.x) - (style.FramePadding.x * 2.0F));
                const float originX = contentWidth < innerWidth
                                          ? min.x + (((max.x - min.x) - contentWidth) * 0.5F)
                                          : min.x + style.FramePadding.x;
                const float midY = (min.y + max.y) * 0.5F;
                const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
                const float thickness = std::max(2.0F * GetDpiScale(), diameter * 0.14F);
                const float radius = std::max(1.0F, (diameter - thickness) * 0.5F);

                ImDrawList *draw = ImGui::GetWindowDrawList();
                draw->PushClipRect(min, max, true);
                PaintSpinner(draw, ImVec2(originX + (diameter * 0.5F), midY), radius, thickness, ink);
                draw->AddText(ImVec2(originX + diameter + gap, midY - (textSize.y * 0.5F)), ink, text);
                draw->PopClipRect();
            }

            if (dimmed) {
                ImGui::EndDisabled();
            }
            return clicked;
        }
    }

    void Spinner(const ImVec4 &color, const float diameter) {
        const float dpi = GetDpiScale();
        const float size = diameter > 0.0F ? diameter : ImGui::GetTextLineHeight();
        const float thickness = std::max(2.0F * dpi, size * 0.12F);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        const float radius = std::max(1.0F, (size - thickness) * 0.5F);
        const ImVec2 center(origin.x + (size * 0.5F), origin.y + (size * 0.5F));
        PaintSpinner(ImGui::GetWindowDrawList(), center, radius, thickness, ImGui::GetColorU32(color));
    }

    namespace {
        const char *NoticeAccent(const NoticeCardTone tone) {
            switch (tone) {
                case NoticeCardTone::Positive:
                    return Colors::POSITIVE;
                case NoticeCardTone::Error:
                    return Colors::NEGATIVE;
                case NoticeCardTone::Warning:
                    return Colors::WARNING;
                case NoticeCardTone::Info:
                    return Colors::ACCENT_INFO;
                case NoticeCardTone::Default:
                default:
                    return nullptr;
            }
        }

        float DrawCenteredWrapped(
            ImDrawList *draw,
            ImFont *font,
            const float fontSize,
            const ImU32 color,
            const char *fullText,
            const float left,
            const float width,
            float y
        ) {
            if (fullText == nullptr || fullText[0] == '\0') {
                return 0.0F;
            }
            const float step = fontSize * (ImGui::GetTextLineHeight() / std::max(1.0F, ImGui::GetFontSize()));
            const char *text = fullText;
            const char *textEnd = fullText + std::strlen(fullText);
            float height = 0.0F;
            while (text < textEnd) {
                const char *wrap = font->CalcWordWrapPosition(fontSize, text, textEnd, width);
                if (wrap <= text) {
                    wrap = textEnd;
                }
                const char *lineEnd = wrap;
                while (lineEnd > text && (lineEnd[-1] == ' ' || lineEnd[-1] == '\t')) {
                    --lineEnd;
                }
                if (draw != nullptr && lineEnd > text) {
                    const ImVec2 size = font->CalcTextSizeA(fontSize, 10000.0F, 0.0F, text, lineEnd);
                    const float x = left + std::max(0.0F, (width - size.x) * 0.5F);
                    draw->AddText(font, fontSize, ImVec2(x, y), color, text, lineEnd);
                }
                height += step;
                y += step;
                text = wrap;
                while (text < textEnd && (*text == ' ' || *text == '\t')) {
                    ++text;
                }
            }
            return height;
        }
    }

    bool NoticeCard(const NoticeCardTone tone, const char *title, const char *body, const char *buttonLabel) {
        if (title == nullptr || title[0] == '\0') {
            return false;
        }

        const bool hasBody = body != nullptr && body[0] != '\0';
        const bool hasButton = buttonLabel != nullptr && buttonLabel[0] != '\0';
        const ImGuiStyle &style = ImGui::GetStyle();
        const float availW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float availH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
        const float cardW = std::min(availW, Em(36.0F));
        const float padX = style.FramePadding.x * 3.0F;
        const float padY = style.FramePadding.y * 3.0F;
        const float rounding = 8.0F * GetDpiScale();
        const float innerW = std::max(1.0F, cardW - (padX * 2.0F));
        const float gap = style.ItemSpacing.y * 1.5F;
        const char *accent = NoticeAccent(tone);
        const char *titleColor = accent != nullptr ? accent : static_cast<const char *>(Colors::TEXT_PRIMARY);
        const char *borderColor = accent != nullptr ? accent : static_cast<const char *>(Colors::BORDER);

        ImFont *font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const float titleSize = fontSize * 1.2F;
        const float titleH = DrawCenteredWrapped(nullptr, font, titleSize, 0, title, 0.0F, innerW, 0.0F);
        const float bodyH = hasBody ? DrawCenteredWrapped(nullptr, font, fontSize, 0, body, 0.0F, innerW, 0.0F) : 0.0F;
        const float buttonH = hasButton ? ImGui::GetFrameHeight() : 0.0F;
        const float buttonW = hasButton
                                  ? std::min(innerW, ImGui::CalcTextSize(buttonLabel).x + (style.FramePadding.x * 4.0F))
                                  : 0.0F;

        float cardH = padY + titleH + padY;
        if (hasBody) {
            cardH += gap + bodyH;
        }
        if (hasButton) {
            cardH += (gap * 2.0F) + buttonH;
        }

        const float top = std::max(0.0F, (availH - cardH) * 0.5F);
        if (top > 0.0F) {
            ImGui::Dummy(ImVec2(0.0F, top));
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (availW - cardW) * 0.5F));

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(cardW, cardH));

        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ImVec2 cardMax(origin.x + cardW, origin.y + cardH);
        // Stroke is centered on the path, so a clip tight to the card rect cuts the outer half.
        // OpenGL's scissor truncates the top of that clip, which made the top edge look thinner.
        const float border = std::max(1.0F, style.FrameBorderSize);
        draw->AddRectFilled(origin, cardMax, ImGui::GetColorU32(HexColor(Colors::SURFACE1)), rounding);
        draw->AddRect(origin, cardMax, ImGui::GetColorU32(HexColor(borderColor)), rounding, border);

        const float textX = origin.x + padX;
        float textY = origin.y + padY;
        textY += DrawCenteredWrapped(
            draw,
            font,
            titleSize,
            ImGui::GetColorU32(HexColor(titleColor)),
            title,
            textX,
            innerW,
            textY
        );
        if (hasBody) {
            textY += gap;
            textY += DrawCenteredWrapped(
                draw,
                font,
                fontSize,
                ImGui::GetColorU32(HexColor(Colors::TEXT_MUTED)),
                body,
                textX,
                innerW,
                textY
            );
        }

        if (!hasButton) {
            return false;
        }

        const float buttonX = origin.x + ((cardW - buttonW) * 0.5F);
        const float buttonY = textY + (gap * 2.0F);
        ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));
        return PrimaryButton(buttonLabel, true, ImVec2(buttonW, 0.0F));
    }

    bool SelectableItem(
        const char *label,
        const bool isSelected,
        const char *rightText,
        const ImVec4 &rightColor,
        const char *leftIcon,
        const ImVec4 &leftIconColor
    ) {
        StyleColor sc;
        StyleVar sv;

        if (isSelected) {
            sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE3, 0.4F));
        } else {
            sc.Push(ImGuiCol_Button, HexColor(Colors::SHADOW, 0.0F));
        }

        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3, 0.4F));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE3, 0.8F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));

        sv.Push(ImGuiStyleVar_FrameRounding, 6.0F);
        sv.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
        sv.Push(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0F, 0.5F));

        std::string buttonLabel;
        if (leftIcon && leftIcon[0] != '\0') {
            buttonLabel = leftIcon;
            buttonLabel += "  ";
            buttonLabel += label;
        } else {
            buttonLabel = label;
        }

        const bool clicked = ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0F, 0.0F));

        if (leftIcon && leftIcon[0] != '\0') {
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            const ImVec2 iconSize = ImGui::CalcTextSize(leftIcon);

            const auto iconPos = ImVec2(
                itemMin.x + padding.x,
                itemMin.y + ((itemMax.y - itemMin.y - iconSize.y) * 0.5F)
            );

            ImGui::GetWindowDrawList()->AddText(
                iconPos,
                ImGui::ColorConvertFloat4ToU32(leftIconColor),
                leftIcon
            );
        }

        if (rightText && rightText[0] != '\0') {
            const ImVec2 textSize = ImGui::CalcTextSize(rightText);
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;

            const auto textPos = ImVec2(
                itemMax.x - textSize.x - padding.x,
                itemMin.y + ((itemMax.y - itemMin.y - textSize.y) * 0.5F)
            );

            ImGui::GetWindowDrawList()->AddText(
                textPos,
                ImGui::ColorConvertFloat4ToU32(rightColor),
                rightText
            );
        }

        return clicked;
    }

    bool PropertyText(const char *label, const char *value, const bool isClickable, const bool hasSpaceBetween) {
        ImGui::TextDisabled("%s", label);

        if (hasSpaceBetween) {
            const float valueWidth = ImGui::CalcTextSize(value).x;
            ImGui::SameLine(
                ImGui::GetContentRegionAvail().x - valueWidth + ImGui::GetCursorPosX() - ImGui::GetCursorStartPos().x
            );
        } else {
            ImGui::SameLine();
        }

        if (!isClickable) {
            ImGui::Text("%s", value);
            return false;
        }

        ImGui::PushID(label);
        const ImVec2 textPos = ImGui::GetCursorScreenPos();
        const ImVec2 textSize = ImGui::CalcTextSize(value);

        const bool clicked = ImGui::InvisibleButton("##link", textSize);
        const bool hovered = ImGui::IsItemHovered();

        const ImU32 color = hovered
                                ? ImGui::ColorConvertFloat4ToU32(HexColor(Colors::ACCENT_INFO_SOFT))
                                : ImGui::ColorConvertFloat4ToU32(HexColor(Colors::ACCENT_INFO));

        ImGui::GetWindowDrawList()->AddText(textPos, color, value);

        if (hovered) {
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(textPos.x, textPos.y + textSize.y),
                ImVec2(textPos.x + textSize.x, textPos.y + textSize.y),
                color
            );
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::PopID();
        return clicked;
    }

    void PropertyTextWrapped(const char *label, const char *value, const bool invertColors) {
        StyleColor sc;
        if (invertColors) {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
        }
        ImGui::Text("%s", label);
        ImGui::SameLine();

        if (invertColors) {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        } else {
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
        }
        ImGui::TextWrapped("%s", value);
    }

    namespace {
        bool CategoryChip(const char *label, const char *fill, const char *accent) {
            StyleColor sc;
            StyleVar sv;
            sc.Push(ImGuiCol_Button, HexColor(fill, 0.16F));
            sc.Push(ImGuiCol_ButtonHovered, HexColor(fill, 0.24F));
            sc.Push(ImGuiCol_ButtonActive, HexColor(fill, 0.32F));
            sc.Push(ImGuiCol_Text, HexColor(accent));
            sc.Push(ImGuiCol_Border, HexColor(accent));
            sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
            sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0F, 5.0F));
            sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);
            return ImGui::Button(label);
        }
    }

    bool CategoryChip(const char *label, const bool isSelected) {
        if (isSelected) {
            return CategoryChip(label, Colors::POSITIVE_FILL, Colors::POSITIVE);
        }
        StyleColor sc;
        StyleVar sv;
        sc.Push(ImGuiCol_Button, HexColor(Colors::SURFACE2));
        sc.Push(ImGuiCol_ButtonHovered, HexColor(Colors::SURFACE3));
        sc.Push(ImGuiCol_ButtonActive, HexColor(Colors::SURFACE4));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_HINT));
        sc.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
        sv.Push(ImGuiStyleVar_FrameRounding, 999.0F);
        sv.Push(ImGuiStyleVar_FramePadding, ImVec2(10.0F, 5.0F));
        sv.Push(ImGuiStyleVar_FrameBorderSize, 1.0F);
        return ImGui::Button(label);
    }

    bool CategoryChip(const char *label, const char *accent) {
        return CategoryChip(label, accent, accent);
    }

    bool CollapsingHeader(const char *label, const ImGuiTreeNodeFlags flags) {
        StyleColor sc;
        sc.Push(ImGuiCol_Header, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_HeaderHovered, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_HeaderActive, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_Border, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_BorderShadow, HexColor(Colors::SHADOW, 0.0F));
        sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_ON_BRIGHT));
        return ImGui::CollapsingHeader(label, flags);
    }

    DialogResult SimpleDialog(const DialogData &data, const bool hasSpinner) {
        auto result = DialogResult::None;
        const std::string title = StrConcat(data.Title, "###", data.Id);

        constexpr ImGuiWindowFlags FLAGS =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking;

        if (!data.IsOpen && !ImGui::IsPopupOpen(title.c_str())) {
            return result;
        }

        if (BeginCenteredModal(title.c_str(), data.IsBusy ? nullptr : &data.IsOpen, ImVec2(Em(42.0F), 0), FLAGS)) {
            if (!data.IsOpen) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return result;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::TextWrapped("%s", data.Message);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Spacing();

            const float halfWidth = EqualButtonWidth(2);
            const bool busy = data.IsBusy;
            const bool showSpinner = busy && hasSpinner;
            const char *confirmLabel = busy && data.BusyButtonTitle != nullptr ? data.BusyButtonTitle : data.ConfirmButtonTitle;
            const ImVec2 buttonSize(halfWidth, 0);

            bool confirmed = false;
            switch (data.Type) {
                case DialogType::Negative:
                    confirmed = NegativeButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
                case DialogType::Positive:
                    confirmed = PositiveButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
                default:
                    confirmed = PrimaryButton(confirmLabel, !busy || showSpinner, buttonSize, showSpinner);
                    break;
            }
            if (confirmed) {
                result = DialogResult::Confirmed;
            }

            ImGui::SameLine();
            if (PrimaryButton(data.CancelButtonTitle, !busy, buttonSize)) {
                result = DialogResult::Cancelled;
                data.IsOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return result;
    }

    MenuStyle::MenuStyle() {
        const float s = GetDpiScale();
        Vars.Push(ImGuiStyleVar_PopupRounding, 6.0F * s);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F * s, 12.0F * s));
        Vars.Push(ImGuiStyleVar_FramePadding, ImVec2(12.0F * s, 10.0F * s));
        Vars.Push(ImGuiStyleVar_ItemSpacing, ImVec2(16.0F * s, 12.0F * s));
    }

    namespace {
        bool RoundedMenuItemImpl(const char *label, const char *shortcut, bool isSelected, bool *pIsSelected, bool isEnabled) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->ChannelsSplit(2);
            drawList->ChannelsSetCurrent(1);

            // Suppress ImGui's built-in (square) hover/active fill; we draw our own.
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

            const bool pressed =
                pIsSelected
                    ? ImGui::MenuItem(label, shortcut, pIsSelected, isEnabled)
                    : ImGui::MenuItem(label, shortcut, isSelected, isEnabled);

            ImGui::PopStyleColor(2);

            drawList->ChannelsSetCurrent(0);
            if (isEnabled && ImGui::IsItemHovered()) {
                const ImVec2 pMin = ImGui::GetItemRectMin();
                const ImVec2 pMax = ImGui::GetItemRectMax();
                const ImVec4 fill = ImGui::IsItemActive() ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
                drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 4.0F * GetDpiScale());
            }
            drawList->ChannelsMerge();
            return pressed;
        }
    }

    bool RoundedMenuItem(const char *label, const char *shortcut, const bool isSelected, const bool isEnabled) {
        return RoundedMenuItemImpl(label, shortcut, isSelected, nullptr, isEnabled);
    }

    bool RoundedMenuItem(const char *label, const char *shortcut, bool *pIsSelected, const bool isEnabled) {
        return RoundedMenuItemImpl(label, shortcut, false, pIsSelected, isEnabled);
    }

    bool RoundedBeginMenu(const char *label, const bool isEnabled) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

        const bool open = ImGui::BeginMenu(label, isEnabled);

        ImGui::PopStyleColor(3);

        drawList->ChannelsSetCurrent(0);
        if (isEnabled && (open || ImGui::IsItemHovered())) {
            const ImVec2 pMin = ImGui::GetItemRectMin();
            const ImVec2 pMax = ImGui::GetItemRectMax();
            const ImVec4 fill = open ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
            drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 50.0F * GetDpiScale());
        }
        drawList->ChannelsMerge();
        return open;
    }

    ComboStyle::ComboStyle() {
        const float s = GetDpiScale();
        Vars.Push(ImGuiStyleVar_PopupRounding, 6.0F * s);
        Vars.Push(ImGuiStyleVar_WindowPadding, ImVec2(14.0F * s, 12.0F * s));
        Vars.Push(ImGuiStyleVar_FramePadding, ImVec2(14.0F * s, 8.0F * s));
        Vars.Push(ImGuiStyleVar_ItemSpacing, ImVec2(16.0F * s, 12.0F * s));
    }

    bool RoundedSelectable(const char *label, const bool isSelected, const ImGuiSelectableFlags flags, const ImVec2 &size) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

        const bool pressed = ImGui::Selectable(label, isSelected, flags, size);

        ImGui::PopStyleColor(3);

        drawList->ChannelsSetCurrent(0);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered || isSelected) {
            const ImVec2 pMin = ImGui::GetItemRectMin();
            const ImVec2 pMax = ImGui::GetItemRectMax();
            const ImVec4 fill = ImGui::IsItemActive() ? HexColor(Colors::SURFACE4) : HexColor(Colors::SURFACE3);
            drawList->AddRectFilled(pMin, pMax, ImGui::GetColorU32(fill), 4.0F * GetDpiScale());
        }
        drawList->ChannelsMerge();
        return pressed;
    }

    float RecentFileItemHeight() {
        return ImGui::GetTextLineHeight() + (ImGui::GetStyle().FramePadding.y * 2.0F);
    }

    float RecentFileItemWidth(const char *label) {
        const float scale = GetDpiScale();
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const float gap = 8.0F * scale;
        const float textW = ImGui::CalcTextSize(label == nullptr ? "" : label).x;
        return padding.x + textW + gap + ImGui::GetTextLineHeight() + padding.x;
    }

    RecentFileAction RecentFileItem(const char *id, const char *label) {
        const float scale = GetDpiScale();
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        const float diameter = ImGui::GetTextLineHeight();
        const ImVec2 size(RecentFileItemWidth(label), RecentFileItemHeight());

        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float rowHitW = std::max(1.0F, size.x - padding.x - diameter);
        const bool rowPressed = ImGui::InvisibleButton("##recent-file", ImVec2(rowHitW, size.y));
        const bool rowHovered = ImGui::IsItemHovered();
        const bool rowHeld = ImGui::IsItemActive();

        const ImVec2 circleMin(origin.x + size.x - padding.x - diameter, origin.y + ((size.y - diameter) * 0.5F));
        const ImVec2 circleMax(circleMin.x + diameter, circleMin.y + diameter);
        ImGui::SetCursorScreenPos(circleMin);
        const bool removePressed = ImGui::InvisibleButton("##recent-remove", ImVec2(diameter, diameter));
        const bool removeHovered = ImGui::IsItemHovered();

        ImDrawList *draw = ImGui::GetWindowDrawList();
        if (rowHovered || rowHeld) {
            const ImVec4 fill = rowHeld ? HexColor(Colors::SURFACE3, 0.8F) : HexColor(Colors::SURFACE3, 0.4F);
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(fill), 6.0F * scale);
        }
        draw->AddText(
            ImVec2(origin.x + padding.x, origin.y + padding.y),
            ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)),
            label
        );

        const ImVec2 center((circleMin.x + circleMax.x) * 0.5F, (circleMin.y + circleMax.y) * 0.5F);
        const ImU32 circle = ImGui::GetColorU32(removeHovered ? HexColor(Colors::NEGATIVE) : HexColor(Colors::SURFACE4));
        draw->AddCircleFilled(center, diameter * 0.5F, circle);
        const ImVec2 iconSize = ImGui::CalcTextSize(Icons::TIMES);
        draw->AddText(
            ImVec2(center.x - (iconSize.x * 0.5F), center.y - (iconSize.y * 0.5F)),
            ImGui::GetColorU32(removeHovered ? HexColor(Colors::WHITE) : HexColor(Colors::TEXT_SUBTLE)),
            Icons::TIMES
        );

        ImGui::PopID();
        if (removePressed) {
            return RecentFileAction::Removed;
        }
        if (rowPressed && !removeHovered) {
            return RecentFileAction::Activated;
        }
        return RecentFileAction::None;
    }

    bool RoundedBeginPopupModal(const char *name, bool *pOpen, const ImGuiWindowFlags flags) {
        const bool open = ImGui::BeginPopupModal(name, nullptr, flags);
        if (!open || pOpen == nullptr) {
            return open;
        }

        const ImGuiStyle &style = ImGui::GetStyle();
        const float s = GetDpiScale();
        const float fontSize = ImGui::GetFontSize();
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();

        const ImVec2 btnMin(
            winPos.x + winSize.x - style.FramePadding.x - fontSize,
            winPos.y + style.FramePadding.y
        );
        const ImVec2 btnMax(btnMin.x + fontSize, btnMin.y + fontSize);
        const ImVec2 bbCenter((btnMin.x + btnMax.x) * 0.5F, (btnMin.y + btnMax.y) * 0.5F);
        const ImVec2 crossCenter(bbCenter.x - 0.5F, bbCenter.y - 0.5F);

        const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool hovered = windowHovered && ImGui::IsMouseHoveringRect(btnMin, btnMax, false);
        const bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), false);

        if (hovered) {
            const float baseRadius = (fontSize * 0.5F) + (4.0F * s);
            const float radius = held ? baseRadius - (1.5F * s) : baseRadius;
            const ImVec4 fill = held
                                    ? HexColor(Colors::NEGATIVE_STRONG)
                                    : HexColor(Colors::NEGATIVE, 0.85F);
            drawList->AddCircleFilled(crossCenter, radius, ImGui::GetColorU32(fill));
        }

        const float crossExtent = (fontSize * 0.5F * 0.7071F) - 1.0F;
        const ImU32 crossCol = hovered
                                   ? ImGui::GetColorU32(HexColor(Colors::WHITE))
                                   : ImGui::GetColorU32(ImGuiCol_Text);
        const float crossThick = 1.0F * s;
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y + crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y - crossExtent),
            crossCol,
            crossThick
        );
        drawList->AddLine(
            ImVec2(crossCenter.x + crossExtent, crossCenter.y - crossExtent),
            ImVec2(crossCenter.x - crossExtent, crossCenter.y + crossExtent),
            crossCol,
            crossThick
        );
        drawList->PopClipRect();

        if (clicked) {
            *pOpen = false;
            ImGui::CloseCurrentPopup();
        }
        return open;
    }

    bool BeginCenteredModal(const char *name, bool *pOpen, const ImVec2 &size, const ImGuiWindowFlags flags) {
        if (!ImGui::IsPopupOpen(name)) {
            ImGui::OpenPopup(name);
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
        return RoundedBeginPopupModal(name, pOpen, flags);
    }

    float EqualButtonWidth(const int count) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float gaps = spacing * static_cast<float>(count - 1);
        return (ImGui::GetContentRegionAvail().x - gaps) / static_cast<float>(count);
    }

    void SearchField(const char *id, const char *hint, char *buffer, const std::size_t bufferSize) {
        ImGui::SetNextItemWidth(-1.0F);
        const std::string searchHint = IconWithLabel(Icons::SEARCH, hint);
        ImGui::InputTextWithHint(id, searchHint.c_str(), buffer, bufferSize);
    }

    bool CategoryChipRow(const char *const *labels, const int count, int &selectedIndex) {
        bool clicked = false;
        for (int i = 0; i < count; ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (CategoryChip(labels[i], selectedIndex == i)) {
                selectedIndex = i;
                clicked = true;
            }
        }
        return clicked;
    }

    bool BeginPickerTable(const char *childId, const char *tableId, const int columns, const float height) {
        ImGui::BeginChild(childId, ImVec2(-1.0F, height), 1, ImGuiWindowFlags_NoScrollbar);
        const bool open = ImGui::BeginTable(tableId, columns, PICKER_TABLE_FLAGS, ImVec2(-1.0F, -1.0F));
        if (open) {
            ImGui::TableSetupScrollFreeze(0, 1);
        }
        return open;
    }

    void EndPickerTable(const bool open) {
        if (open) {
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    std::string PathPicker(
        const char *id,
        const char *label,
        const char *hint,
        const char *dialogTitle,
        char *buffer,
        const std::size_t bufferSize,
        const float fieldWidth
    ) {
        const char *browseText = Tr("Browse...");
        const std::string browseLabel = StrConcat(browseText, "##", id);
        const float browseWidth = std::max(
            Em(11.0F),
            ImGui::CalcTextSize(browseText).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
        );
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::Text("%s", label);
        const float totalWidth = fieldWidth > 0.0F ? fieldWidth : ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(std::max(1.0F, totalWidth - browseWidth - spacing));
        ImGui::InputTextWithHint(id, hint, buffer, bufferSize);
        ImGui::SameLine();
        if (PrimaryButton(browseLabel.c_str(), true, ImVec2(browseWidth, 0))) {
            if (const auto picked = FileDialog::PickDirectory(dialogTitle, buffer)) {
                CopyToBuffer(buffer, bufferSize, *picked);
            }
        }
        return buffer;
    }

    bool SubtitledCheckbox(const char *id, bool *value, const char *label, const char *subtitle, const char *tooltip, float boxSize) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        const ImGuiStyle &style = ImGui::GetStyle();
        const ImGuiID itemId = window->GetID(id);
        const ImVec2 titleSize = ImGui::CalcTextSize(label, nullptr, true);
        const ImVec2 subSize = subtitle ? ImGui::CalcTextSize(subtitle, nullptr, true) : ImVec2(0, 0);

        const float textBlockH = subtitle ? titleSize.y + style.ItemInnerSpacing.y + subSize.y : titleSize.y;
        const float rowH = ImMax(boxSize, textBlockH);

        const ImVec2 pos = window->DC.CursorPos;
        const float textW = ImMax(titleSize.x, subSize.x);
        const ImVec2 size(boxSize + style.ItemInnerSpacing.x + textW, rowH);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

        ImGui::ItemSize(size, 0.0F);
        if (!ImGui::ItemAdd(bb, itemId)) {
            return false;
        }

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, itemId, &hovered, &held);
        if (pressed) {
            *value = !*value;
            ImGui::MarkItemEdited(itemId);
        }

        const ImVec2 boxMin(pos.x, pos.y + ((rowH - boxSize) * 0.5F));
        const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
        ImU32 bg = 0;
        if (held && hovered) {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBgActive);
        } else if (hovered) {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
        } else {
            bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
        }
        window->DrawList->AddRectFilled(boxMin, boxMax, bg, style.FrameRounding);
        if (style.FrameBorderSize > 0.0F) {
            window->DrawList->AddRect(
                boxMin,
                boxMax,
                ImGui::GetColorU32(ImGuiCol_Border),
                style.FrameRounding,
                0,
                style.FrameBorderSize
            );
        }
        if (*value) {
            const float pad = ImMax(1.0F, ImFloor(boxSize / 6.0F));
            ImGui::RenderCheckMark(
                window->DrawList,
                ImVec2(boxMin.x + pad, boxMin.y + pad),
                ImGui::GetColorU32(ImGuiCol_CheckMark),
                boxSize - (pad * 2.0F)
            );
        }

        const float textX = boxMax.x + style.ItemInnerSpacing.x;
        if (subtitle) {
            const float textY = pos.y + ((rowH - textBlockH) * 0.5F);
            window->DrawList->AddText(
                ImVec2(textX, textY),
                ImGui::GetColorU32(ImGuiCol_Text),
                label
            );
            window->DrawList->AddText(
                ImVec2(textX, textY + titleSize.y + style.ItemInnerSpacing.y),
                ImGui::GetColorU32(ImGuiCol_TextDisabled),
                subtitle
            );
        } else {
            const float textY = pos.y + ((rowH - titleSize.y) * 0.5F);
            window->DrawList->AddText(
                ImVec2(textX, textY),
                ImGui::GetColorU32(ImGuiCol_Text),
                label
            );
        }

        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", tooltip);
        }

        return pressed;
    }

    bool StatusActionItem(
        const char *id,
        const char *icon,
        const ImVec4 &iconColor,
        const char *title,
        const char *description,
        const ImVec4 &descriptionColor,
        const char *actionLabel,
        const bool actionEnabled
    ) {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        ImGui::PushID(id);

        const ImGuiStyle &style = ImGui::GetStyle();
        const float avail = ImGui::GetContentRegionAvail().x;
        const bool hasAction = actionLabel != nullptr && actionLabel[0] != '\0';
        const ImVec2 actionSize = hasAction ? ImGui::CalcTextSize(actionLabel) : ImVec2(0.0F, 0.0F);
        const float buttonW = hasAction ? actionSize.x + (style.FramePadding.x * 2.0F) : 0.0F;
        const float buttonH = hasAction ? actionSize.y + (style.FramePadding.y * 2.0F) : 0.0F;

        const float padX = style.FramePadding.x;
        const float padY = style.FramePadding.y;
        const float iconGap = style.ItemSpacing.x;
        const float actionGap = hasAction ? style.ItemSpacing.x : 0.0F;
        const ImVec2 iconSize = ImGui::CalcTextSize(icon != nullptr ? icon : "");
        const float textW = ImMax(1.0F, avail - (padX * 2.0F) - iconSize.x - iconGap - actionGap - buttonW);

        const ImVec2 titleSize = ImGui::CalcTextSize(title, nullptr, false, textW);
        const bool hasDescription = description != nullptr && description[0] != '\0';
        const ImVec2 descSize = hasDescription ? ImGui::CalcTextSize(description, nullptr, false, textW) : ImVec2(0.0F, 0.0F);
        const float textGap = hasDescription ? style.ItemInnerSpacing.y : 0.0F;
        const float textBlockH = titleSize.y + textGap + descSize.y;
        const float contentH = ImMax(textBlockH, ImMax(iconSize.y, buttonH));
        const float rowH = contentH + (padY * 2.0F);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + avail, pos.y + rowH));
        const ImGuiID itemId = window->GetID("##status_action");

        ImGui::ItemSize(ImVec2(avail, rowH));
        if (!ImGui::ItemAdd(bb, itemId)) {
            ImGui::PopID();
            return false;
        }

        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(HexColor(Colors::SURFACE2)), style.FrameRounding);
        window->DrawList->AddRect(
            bb.Min,
            bb.Max,
            ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE, 0.5F)),
            style.FrameRounding,
            0,
            1.0F
        );

        const float textX = bb.Min.x + padX + iconSize.x + iconGap;
        const float textY = bb.Min.y + padY + ((contentH - textBlockH) * 0.5F);
        ImFont *font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();

        if (icon != nullptr && icon[0] != '\0') {
            const float iconY = bb.Min.y + padY + ((contentH - iconSize.y) * 0.5F);
            window->DrawList->AddText(ImVec2(bb.Min.x + padX, iconY), ImGui::GetColorU32(iconColor), icon);
        }

        window->DrawList->AddText(font, fontSize, ImVec2(textX, textY), ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)), title, nullptr, textW);
        if (hasDescription) {
            window->DrawList->AddText(
                font,
                fontSize,
                ImVec2(textX, textY + titleSize.y + textGap),
                ImGui::GetColorU32(descriptionColor),
                description,
                nullptr,
                textW
            );
        }

        bool pressed = false;
        if (hasAction) {
            const float buttonX = bb.Max.x - padX - buttonW;
            const float buttonY = bb.Min.y + ((rowH - buttonH) * 0.5F);
            ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));
            pressed = PrimaryButton(actionLabel, actionEnabled, ImVec2(buttonW, buttonH));
            ImGui::SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y + style.ItemSpacing.y));
        }

        ImGui::PopID();
        return pressed;
    }

    namespace {
        struct BannerPalette {
            const char *Surface;
            const char *Ink;
            const char *Detail;
            const char *Accent;
            const char *AccentStrong;
            const char *AccentHover;
            const char *AccentActive;
        };

        BannerPalette PaletteFor(const BannerTone tone) {
            if (IsLightColorScheme()) {
                switch (tone) {
                    case BannerTone::Info:
                        return {
                            .Surface = "#E8F1FC",
                            .Ink = Colors::WHITE,
                            .Detail = "#1E3A5F",
                            .Accent = Colors::ACCENT_INFO,
                            .AccentStrong = Colors::ACCENT_INFO,
                            .AccentHover = "#2563EB",
                            .AccentActive = "#1E3A8A",
                        };
                    case BannerTone::Positive:
                        return {
                            .Surface = "#E7F6EA",
                            .Ink = Colors::WHITE,
                            .Detail = "#14532D",
                            .Accent = Colors::POSITIVE,
                            .AccentStrong = Colors::POSITIVE,
                            .AccentHover = "#1A8740",
                            .AccentActive = "#0F5C28",
                        };
                    case BannerTone::Warning:
                    default:
                        return {
                            .Surface = "#FBF3E4",
                            .Ink = Colors::WHITE,
                            .Detail = "#6B5420",
                            .Accent = Colors::WARNING,
                            .AccentStrong = Colors::WARNING,
                            .AccentHover = "#9A6808",
                            .AccentActive = "#6A4C05",
                        };
                }
            }

            switch (tone) {
                case BannerTone::Info:
                    return {
                        .Surface = "#161C28",
                        .Ink = "#081018",
                        .Detail = "#B7C6D6",
                        .Accent = Colors::ACCENT_INFO,
                        .AccentStrong = Colors::ACCENT_INFO_SOFT,
                        .AccentHover = "#8EC4FF",
                        .AccentActive = "#2E6FDB",
                    };
                case BannerTone::Positive:
                    return {
                        .Surface = "#16241C",
                        .Ink = "#08140C",
                        .Detail = "#B7D4C4",
                        .Accent = Colors::POSITIVE,
                        .AccentStrong = "#5AD66A",
                        .AccentHover = "#7AE088",
                        .AccentActive = "#1F9A32",
                    };
                case BannerTone::Warning:
                default:
                    return {
                        .Surface = "#241E16",
                        .Ink = "#1A1408",
                        .Detail = "#D2C4AA",
                        .Accent = Colors::WARNING,
                        .AccentStrong = Colors::WARNING_STRONG,
                        .AccentHover = "#F0D056",
                        .AccentActive = "#C49A12",
                    };
            }
        }

        bool BannerPrimaryButton(const char *label, const ImVec2 &size, const BannerPalette &palette) {
            StyleColor colors;
            colors.Push(ImGuiCol_Button, HexColor(palette.AccentStrong));
            colors.Push(ImGuiCol_ButtonHovered, HexColor(palette.AccentHover));
            colors.Push(ImGuiCol_ButtonActive, HexColor(palette.AccentActive));
            colors.Push(ImGuiCol_Text, HexColor(palette.Ink));
            colors.Push(ImGuiCol_Border, HexColor(palette.AccentStrong));

            StyleVar vars;
            vars.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
            return ImGui::Button(label, size);
        }

        bool BannerQuietButton(const char *label, const ImVec2 &size, const BannerPalette &palette) {
            StyleColor colors;
            colors.Push(ImGuiCol_Button, HexColor(palette.Accent, 0.0F));
            colors.Push(ImGuiCol_ButtonHovered, HexColor(palette.Accent, 0.16F));
            colors.Push(ImGuiCol_ButtonActive, HexColor(palette.Accent, 0.28F));
            colors.Push(ImGuiCol_Text, HexColor(palette.Detail));
            colors.Push(ImGuiCol_Border, HexColor(palette.Accent, 0.0F));

            StyleVar vars;
            vars.Push(ImGuiStyleVar_FrameBorderSize, 0.0F);
            return ImGui::Button(label, size);
        }

        void DrawBannerChrome(const float accentW, const BannerPalette &palette) {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            drawList->AddRectFilled(
                pos,
                ImVec2(pos.x + accentW, pos.y + size.y),
                ImGui::GetColorU32(HexColor(palette.Accent))
            );
            drawList->AddLine(
                ImVec2(pos.x, pos.y + size.y - 1.0F),
                ImVec2(pos.x + size.x, pos.y + size.y - 1.0F),
                ImGui::GetColorU32(HexColor(palette.Accent, 0.45F)),
                1.0F
            );
        }
    }

    namespace {
        bool HasText(const char *text) {
            return text != nullptr && text[0] != '\0';
        }

        void DrawTaskLine(const char *text, const float width, const float columnX, const ImVec4 &color, const bool center) {
            float x = columnX;
            if (center) {
                const float textW = ImGui::CalcTextSize(text).x;
                x += std::max(0.0F, (width - textW) * 0.5F);
            }
            ImGui::SetCursorPosX(x);
            ImGui::TextColored(color, "%s", text);
        }
    }

    bool TaskProgressPanel(const TaskProgress &task) {
        const bool center = task.CenterHorizontally;
        const float availW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float width = center ? std::min(Em(66.0F), availW) : availW;
        const float columnX = ImGui::GetCursorPosX() + (center ? std::max(0.0F, (availW - width) * 0.5F) : 0.0F);
        const bool hasTitle = HasText(task.Title);
        const bool hasSubtitle = HasText(task.Subtitle);
        const bool hasStatus = HasText(task.Status);
        const bool hasDetail = HasText(task.Detail);
        const bool hasCancel = HasText(task.CancelLabel);
        const float gap = ImGui::GetStyle().ItemSpacing.y;
        const float line = ImGui::GetTextLineHeightWithSpacing();

        if (task.CenterVertically) {
            float blockH = ImGui::GetFrameHeight();
            if (hasTitle) {
                blockH += line;
            }
            if (hasSubtitle) {
                blockH += gap + line;
            }
            if (hasTitle || hasSubtitle) {
                blockH += gap * (task.CenterHorizontally ? 3.0F : 1.0F);
            }
            if (hasStatus) {
                blockH += gap + line;
            }
            if (hasDetail) {
                blockH += gap + ImGui::CalcTextSize(task.Detail, nullptr, false, width).y;
            }
            if (hasCancel) {
                blockH += (gap * 3.0F) + ImGui::GetFrameHeight();
            }
            const float availH = ImGui::GetContentRegionAvail().y;
            if (availH > blockH) {
                ImGui::Dummy(ImVec2(0.0F, (availH - blockH) * 0.5F));
            }
        }

        ImGui::SetCursorPosX(columnX);
        ImGui::BeginGroup();

        if (hasTitle) {
            DrawTaskLine(task.Title, width, columnX, HexColor(Colors::TEXT_PRIMARY), center);
            if (HasText(task.TitleTooltip) && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", task.TitleTooltip);
            }
        }
        if (hasSubtitle) {
            if (hasTitle) {
                ImGui::Spacing();
            }
            DrawTaskLine(task.Subtitle, width, columnX, HexColor(Colors::TEXT_MUTED), center);
        }
        if (hasTitle || hasSubtitle) {
            ImGui::Spacing();
            if (center) {
                ImGui::Spacing();
                ImGui::Spacing();
            }
        }

        ImGui::SetCursorPosX(columnX);
        ImGui::ProgressBar(std::clamp(task.Fraction, 0.0F, 1.0F), ImVec2(width, 0.0F));

        if (hasStatus) {
            ImGui::Spacing();
            ImGui::SetCursorPosX(columnX);
            ImGui::TextColored(HexColor(Colors::TEXT_PRIMARY), "%s", task.Status);
        }
        if (hasDetail) {
            ImGui::SetCursorPosX(columnX);
            ImGui::PushTextWrapPos(columnX + width);
            ImGui::TextColored(HexColor(Colors::TEXT_MUTED), "%s", task.Detail);
            ImGui::PopTextWrapPos();
        }

        bool cancelPressed = false;
        if (hasCancel) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
            const float shownW = ImGui::CalcTextSize(task.CancelLabel).x;
            const float reservedW = HasText(task.CancelSizingLabel) ? ImGui::CalcTextSize(task.CancelSizingLabel).x : 0.0F;
            const float buttonW = std::max(Em(14.0F), std::max(shownW, reservedW) + (ImGui::GetStyle().FramePadding.x * 2.0F));
            const float buttonX = center ? columnX + std::max(0.0F, (width - buttonW) * 0.5F) : columnX;
            ImGui::SetCursorPosX(buttonX);
            cancelPressed = NegativeButton(task.CancelLabel, task.CancelEnabled, ImVec2(buttonW, 0.0F));
        }

        ImGui::EndGroup();
        return cancelPressed;
    }

    BannerResult ShowBanner(const Banner &banner) {
        if (banner.Id == nullptr || banner.Id[0] == '\0' || banner.Title == nullptr || banner.Title[0] == '\0') {
            return BannerResult::None;
        }

        const BannerPalette palette = PaletteFor(banner.Tone);
        const bool hasIcon = banner.Icon != nullptr && banner.Icon[0] != '\0';
        const bool hasSubtitle = banner.Subtitle != nullptr && banner.Subtitle[0] != '\0';
        const bool hasAction = banner.ActionLabel != nullptr && banner.ActionLabel[0] != '\0';
        const bool hasDismiss = banner.Dismissable;
        const bool hasButtons = hasAction || hasDismiss;

        const ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, 0.0F));

        constexpr ImGuiWindowFlags FLAGS =
            WINDOW_AUTO_RESIZE_FLAGS |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        const float dpi = GetDpiScale();
        const float accentW = 3.0F * dpi;
        const ImGuiStyle &themeStyle = ImGui::GetStyle();
        const float tooltipRounding = themeStyle.WindowRounding;
        const float tooltipBorder = themeStyle.WindowBorderSize;
        const ImVec2 tooltipPadding = themeStyle.WindowPadding;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, HexColor(palette.Surface));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2((14.0F * dpi) + accentW, 12.0F * dpi));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::Begin(banner.Id, nullptr, FLAGS);

        const ImGuiStyle &style = ImGui::GetStyle();
        const float spacing = style.ItemSpacing.x;
        const ImVec2 actionText = hasAction ? ImGui::CalcTextSize(banner.ActionLabel) : ImVec2(0.0F, 0.0F);
        const float labelH = hasAction ? actionText.y : ImGui::CalcTextSize(Icons::TIMES).y;
        const float buttonH = labelH + (style.FramePadding.y * 2.0F);
        const float actionW = hasAction ? actionText.x + (style.FramePadding.x * 2.0F) : 0.0F;
        const float dismissW = hasDismiss ? buttonH : 0.0F;
        float buttonsW = actionW + dismissW;
        if (hasAction && hasDismiss) {
            buttonsW += spacing;
        }

        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 iconSize = hasIcon ? ImGui::CalcTextSize(banner.Icon) : ImVec2(0.0F, 0.0F);
        const float iconGap = hasIcon ? style.ItemInnerSpacing.x : 0.0F;
        const float textGap = hasSubtitle ? style.ItemInnerSpacing.y : 0.0F;
        const float leading = iconSize.x + iconGap;
        const float trailing = hasButtons ? spacing + buttonsW : 0.0F;
        const bool sideBySide = !hasButtons || avail > (leading + trailing + Em(16.0F));
        const float textW = sideBySide
                                ? ImMax(1.0F, avail - trailing - leading)
                                : ImMax(1.0F, avail - leading);

        const ImVec2 titleSize = ImGui::CalcTextSize(banner.Title, nullptr, false, textW);
        const ImVec2 detailSize = hasSubtitle
                                      ? ImGui::CalcTextSize(banner.Subtitle, nullptr, false, textW)
                                      : ImVec2(0.0F, 0.0F);
        const float textBlockH = titleSize.y + (hasSubtitle ? textGap + detailSize.y : 0.0F);
        const float rowH = sideBySide && hasButtons ? ImMax(textBlockH, buttonH) : textBlockH;
        const ImVec2 origin = ImGui::GetCursorScreenPos();

        if (hasIcon) {
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + ((titleSize.y - iconSize.y) * 0.5F)));
            ImGui::TextColored(HexColor(palette.AccentStrong), "%s", banner.Icon);
        }

        const float textX = origin.x + leading;
        ImGui::SetCursorScreenPos(ImVec2(textX, origin.y));
        ImGui::PushTextWrapPos(textX + textW);
        ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        ImGui::TextWrapped("%s", banner.Title);
        ImGui::PopStyleColor();

        if (hasSubtitle) {
            ImGui::SetCursorScreenPos(ImVec2(textX, origin.y + titleSize.y + textGap));
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(palette.Detail));
            ImGui::TextWrapped("%s", banner.Subtitle);
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();

        BannerResult result = BannerResult::None;
        if (hasButtons) {
            const float buttonsY = sideBySide
                                       ? origin.y + ((rowH - buttonH) * 0.5F)
                                       : origin.y + textBlockH + style.ItemSpacing.y;
            const float buttonsX = (buttonsW >= avail) ? origin.x : origin.x + avail - buttonsW;
            ImGui::SetCursorScreenPos(ImVec2(buttonsX, buttonsY));
            if (hasAction && BannerPrimaryButton(banner.ActionLabel, ImVec2(actionW, buttonH), palette)) {
                result = BannerResult::Action;
            }
            if (hasDismiss) {
                if (hasAction) {
                    ImGui::SameLine();
                }
                if (BannerQuietButton(Icons::TIMES, ImVec2(dismissW, buttonH), palette)) {
                    result = BannerResult::Dismissed;
                }
                if (banner.DismissTooltip != nullptr && banner.DismissTooltip[0] != '\0' && ImGui::IsItemHovered()) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, tooltipPadding);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tooltipRounding);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, tooltipBorder);
                    ImGui::SetTooltip("%s", banner.DismissTooltip);
                    ImGui::PopStyleVar(3);
                }
            }
        }

        const float usedH = sideBySide ? rowH : textBlockH + style.ItemSpacing.y + buttonH;
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + usedH));
        ImGui::Dummy(ImVec2(avail, 0.0F));

        DrawBannerChrome(accentW, palette);

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        return result;
    }
}

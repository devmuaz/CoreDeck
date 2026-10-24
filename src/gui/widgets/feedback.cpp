//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>
#include <cstring>

#include "imgui.h"
#include "imgui_internal.h"

#include "../../core/i18n.h"
#include "../widgets.h"
#include "internal.h"

namespace CoreDeck::Gui {
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

        float CenteredWrappedText(
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

    float StatusBarHeight() {
        const float padY = 6.0F * GetDpiScale();
        return ImGui::GetTextLineHeight() + (padY * 2.0F);
    }

    void StatusBar(const StatusBarTone tone, const char *message) {
        const char *shown = message != nullptr ? message : "";
        const char *icon = Icons::INFO;
        const char *color = Colors::TEXT_MUTED;
        const char *fill = Colors::SURFACE2;
        const char *border = Colors::SURFACE4;
        float fillAlpha = 1.0F;
        float borderAlpha = 1.0F;
        switch (tone) {
            case StatusBarTone::Success:
                icon = Icons::CHECK_CIRCLE;
                color = Colors::POSITIVE;
                fill = Colors::POSITIVE;
                border = Colors::POSITIVE;
                fillAlpha = 0.12F;
                borderAlpha = 0.35F;
                break;
            case StatusBarTone::Warning:
                icon = Icons::WARNING_TRIANGLE;
                color = Colors::WARNING;
                fill = Colors::WARNING;
                border = Colors::WARNING;
                fillAlpha = 0.12F;
                borderAlpha = 0.35F;
                break;
            case StatusBarTone::Error:
                icon = Icons::TIMES_CIRCLE;
                color = Colors::NEGATIVE;
                fill = Colors::NEGATIVE;
                border = Colors::NEGATIVE;
                fillAlpha = 0.12F;
                borderAlpha = 0.35F;
                break;
            case StatusBarTone::Busy:
                icon = Icons::INFO;
                color = Colors::ACCENT_INFO;
                fill = Colors::ACCENT_INFO;
                border = Colors::ACCENT_INFO;
                fillAlpha = 0.12F;
                borderAlpha = 0.35F;
                break;
            case StatusBarTone::Idle:
                break;
        }

        const float dpi = GetDpiScale();
        const float padX = 10.0F * dpi;
        const float gap = 8.0F * dpi;
        const float height = StatusBarHeight();
        const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));

        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ImVec2 max(origin.x + width, origin.y + height);
        const float rounding = 6.0F * dpi;
        draw->AddRectFilled(origin, max, ImGui::GetColorU32(HexColor(fill, fillAlpha)), rounding);
        draw->AddRect(origin, max, ImGui::GetColorU32(HexColor(border, borderAlpha)), rounding, 0, 1.0F * dpi);

        const ImU32 ink = ImGui::GetColorU32(HexColor(color));
        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        const float iconY = origin.y + ((height - iconSize.y) * 0.5F);
        const float textX = origin.x + padX + iconSize.x + gap;
        const float textWrap = std::max(1.0F, width - (textX - origin.x) - padX);
        draw->AddText(ImVec2(origin.x + padX, iconY), ink, icon);
        draw->PushClipRect(origin, max, true);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textX, origin.y + ((height - ImGui::GetTextLineHeight()) * 0.5F)), ink, shown, nullptr, textWrap);
        draw->PopClipRect();
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
        const float padX = style.FramePadding.x * 3.0F;
        const float padY = style.FramePadding.y * 3.0F;
        const float rounding = 8.0F * GetDpiScale();
        const float gap = style.ItemSpacing.y * 1.5F;
        const char *accent = NoticeAccent(tone);
        const char *titleColor = accent != nullptr ? accent : static_cast<const char *>(Colors::TEXT_PRIMARY);
        const char *borderColor = accent != nullptr ? accent : static_cast<const char *>(Colors::BORDER);

        ImFont *font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const float titleSize = fontSize * 1.2F;
        const float titleLineH = titleSize * (ImGui::GetTextLineHeight() / std::max(1.0F, fontSize));
        const float titleW = std::max(1.0F, font->CalcTextSizeA(titleSize, 10000.0F, 0.0F, title).x);
        const float buttonNatural = hasButton
                                        ? std::max(
                                              Em(16.0F),
                                              ImGui::CalcTextSize(buttonLabel).x + (style.FramePadding.x * 6.0F)
                                          )
                                        : 0.0F;
        const float maxInner = std::max(1.0F, availW - (padX * 2.0F));
        const float innerW = std::min(maxInner, std::max(titleW, buttonNatural));
        const float bodyWrap = std::min(innerW, titleW);
        const float cardW = innerW + (padX * 2.0F);
        const bool titleFits = titleW <= innerW + 0.5F;
        const float titleH = titleFits
                                 ? titleLineH
                                 : CenteredWrappedText(nullptr, font, titleSize, 0, title, 0.0F, innerW, 0.0F);
        const float bodyH = hasBody ? CenteredWrappedText(nullptr, font, fontSize, 0, body, 0.0F, bodyWrap, 0.0F) : 0.0F;
        const float buttonH = hasButton ? ImGui::GetFrameHeight() : 0.0F;
        const float buttonW = hasButton ? std::min(innerW, buttonNatural) : 0.0F;

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
        draw->AddRectFilled(origin, cardMax, ImGui::GetColorU32(HexColor(Colors::SURFACE1)), rounding);
        draw->AddRect(origin, cardMax, ImGui::GetColorU32(HexColor(borderColor)), rounding, 0.5F);

        const float textX = origin.x + padX;
        float textY = origin.y + padY;
        const ImU32 titleInk = ImGui::GetColorU32(HexColor(titleColor));
        if (titleFits) {
            const float titleX = textX + std::max(0.0F, (innerW - titleW) * 0.5F);
            draw->AddText(font, titleSize, ImVec2(titleX, textY), titleInk, title);
            textY += titleLineH;
        } else {
            textY += CenteredWrappedText(draw, font, titleSize, titleInk, title, textX, innerW, textY);
        }
        if (hasBody) {
            textY += gap;
            const float bodyX = textX + std::max(0.0F, (innerW - bodyWrap) * 0.5F);
            textY += CenteredWrappedText(
                draw,
                font,
                fontSize,
                ImGui::GetColorU32(HexColor(Colors::TEXT_MUTED)),
                body,
                bodyX,
                bodyWrap,
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

    void CenteredMessage(const char *title, const char *subtitle, const char *titleColor) {
        if (title == nullptr || title[0] == '\0') {
            return;
        }
        const bool hasSubtitle = subtitle != nullptr && subtitle[0] != '\0';
        const char *ink = titleColor != nullptr && titleColor[0] != '\0' ? titleColor : static_cast<const char *>(Colors::TEXT_PRIMARY);
        const float availW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
        const float availH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
        ImGui::SetWindowFontScale(1.35F);
        const ImVec2 titleSize = ImGui::CalcTextSize(title, nullptr, false, availW);
        ImGui::SetWindowFontScale(1.0F);
        const ImVec2 subtitleSize = hasSubtitle ? ImGui::CalcTextSize(subtitle, nullptr, false, availW) : ImVec2(0.0F, 0.0F);
        const float gap = hasSubtitle ? ImGui::GetStyle().ItemSpacing.y : 0.0F;
        const float blockH = titleSize.y + gap + subtitleSize.y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0F, (availH - blockH) * 0.5F));

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availW);
        ImGui::SetWindowFontScale(1.35F);
        {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(ink));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (availW - titleSize.x) * 0.5F));
            ImGui::TextWrapped("%s", title);
        }
        ImGui::SetWindowFontScale(1.0F);
        if (hasSubtitle) {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (availW - subtitleSize.x) * 0.5F));
            ImGui::TextWrapped("%s", subtitle);
        }
        ImGui::PopTextWrapPos();
    }

    LoadingOverlay::LoadingOverlay(const bool active, const char *label) {
        if (!active) {
            return;
        }
        const ImGuiWindow *window = ImGui::GetCurrentWindow();
        m_Active = true;
        m_WindowId = window->ID;
        m_Label = label != nullptr && label[0] != '\0' ? label : Core::Tr("Loading...");
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        if (GImGui->ActiveId != 0 && GImGui->ActiveIdWindow == window) {
            ImGui::ClearActiveID();
        }
    }

    LoadingOverlay::~LoadingOverlay() {
        End();
    }

    void LoadingOverlay::End() {
        if (!m_Active) {
            return;
        }
        m_Active = false;
        ImGui::PopItemFlag();
        ImGui::PopItemFlag();

        const ImGuiWindow *window = ImGui::FindWindowByID(m_WindowId);
        if (window == nullptr || window->Size.x <= 1.0F || window->Size.y <= 1.0F) {
            return;
        }
        if (m_Label == nullptr || m_Label[0] == '\0') {
            return;
        }

        const float dpi = GetDpiScale();
        const float spinner = 32.0F * dpi;
        const float padX = 22.0F * dpi;
        const float padY = 18.0F * dpi;
        const float gap = 10.0F * dpi;
        const float rounding = ImGui::GetStyle().FrameRounding;
        const float maxTextW = std::max(spinner, window->Size.x - (padX * 2.0F) - (32.0F * dpi));
        const ImVec2 textSize = ImGui::CalcTextSize(m_Label, nullptr, false, maxTextW);
        const float innerW = std::max(spinner, textSize.x);
        const float cardW = innerW + (padX * 2.0F);
        const float cardH = padY + spinner + gap + textSize.y + padY;

        const ImVec2 windowMin = window->Pos;
        const ImVec2 windowMax(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);
        const ImVec2 cardMin(
            windowMin.x + ((window->Size.x - cardW) * 0.5F),
            windowMin.y + ((window->Size.y - cardH) * 0.5F)
        );
        const ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);

        ImDrawList *draw = window->DrawList;
        draw->PushClipRect(windowMin, windowMax, false);
        draw->AddRectFilled(windowMin, windowMax, ImGui::GetColorU32(HexColor(Colors::SHADOW, 0.45F)), window->WindowRounding);
        draw->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(HexColor(Colors::SURFACE1)), rounding);
        draw->AddRect(cardMin, cardMax, ImGui::GetColorU32(HexColor(Colors::BORDER)), rounding, 0, std::max(1.0F, dpi));

        const float thickness = std::max(2.5F * dpi, spinner * 0.10F);
        const float radius = std::max(1.0F, (spinner - thickness) * 0.5F);
        const ImVec2 spinnerCenter(cardMin.x + (cardW * 0.5F), cardMin.y + padY + (spinner * 0.5F));
        PaintSpinner(draw, spinnerCenter, radius, thickness, ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)));

        const float textTop = cardMin.y + padY + spinner + gap;
        CenteredWrappedText(
            draw,
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY)),
            m_Label,
            cardMin.x + padX,
            innerW,
            textTop
        );
        draw->PopClipRect();
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
}

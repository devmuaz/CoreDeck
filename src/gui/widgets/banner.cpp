//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include <algorithm>

#include "imgui.h"
#include "imgui_internal.h"

#include "../widgets.h"

namespace CoreDeck {
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

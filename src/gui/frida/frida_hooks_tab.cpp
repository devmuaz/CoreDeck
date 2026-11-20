//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_hooks_tab.h"

#include <algorithm>
#include <string>

#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        void HookToolbar(Context &context, FridaDeviceSession &session) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const std::string hookLabel = IconWithLabel(Icons::PLUG, Platform::Tr("Hook"));
            const float hookW = ActionWidth(hookLabel.c_str());
            SubtitledCheckbox("##FridaHookNative", &session.HookNative, Platform::Tr("Native"));
            ImGui::SameLine(0.0F, spacing);
            SubtitledCheckbox("##FridaHookArgs", &session.HookLogArgs, Platform::Tr("Log Arguments"));
            ImGui::SameLine(0.0F, spacing);
            SubtitledCheckbox("##FridaHookTrace", &session.HookBacktrace, Platform::Tr("Backtrace"));
            ImGui::SameLine(0.0F, spacing);

            const int fieldCount = session.HookNative ? 3 : 2;
            const float fields = std::max(1.0F, ImGui::GetContentRegionAvail().x - hookW - spacing);
            const float fieldW = std::max(1.0F, (fields - (spacing * static_cast<float>(fieldCount - 1))) / static_cast<float>(fieldCount));
            if (session.HookNative) {
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputTextWithHint("##FridaHookModule", Platform::Tr("Module"), session.HookModule, sizeof(session.HookModule));
                HoverTooltip(Platform::Tr("Module"));
                ImGui::SameLine(0.0F, spacing);
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputTextWithHint("##FridaHookSymbol", Platform::Tr("Symbol"), session.HookSymbol, sizeof(session.HookSymbol));
                HoverTooltip(Platform::Tr("Symbol"));
                ImGui::SameLine(0.0F, spacing);
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputTextWithHint("##FridaHookAddress", Platform::Tr("Address"), session.HookAddress, sizeof(session.HookAddress));
                HoverTooltip(Platform::Tr("Address"));
            } else {
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputTextWithHint("##FridaHookClass", Platform::Tr("Class"), session.HookClass, sizeof(session.HookClass));
                HoverTooltip(Platform::Tr("Class"));
                ImGui::SameLine(0.0F, spacing);
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputTextWithHint("##FridaHookMethod", Platform::Tr("Method"), session.HookMethod, sizeof(session.HookMethod));
                HoverTooltip(Platform::Tr("Method"));
            }
            ImGui::SameLine(0.0F, spacing);
            const bool ready = session.HookNative
                                   ? session.Attached
                                   : session.Attached && session.HookClass[0] != '\0' && session.HookMethod[0] != '\0';
            if (PrimaryButton(hookLabel.c_str(), ready, ImVec2(hookW, 0.0F))) {
                if (session.HookNative) {
                    RequestNativeHook(context, session);
                } else {
                    RequestJavaHook(context, session);
                }
            }
        }

        bool HookRow(const FridaHookRow &hook) {
            const float scale = GetDpiScale();
            const float extra = 4.0F * scale;
            const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float diameter = IconCircleButtonDiameter();
            const float textH = ImGui::GetTextLineHeight();
            const float padX = ImGui::GetStyle().FramePadding.x + extra;
            const std::string rowId = "##FridaHook" + std::to_string(hook.Id);
            const DashboardRowResult row = DashboardRow(
                rowId.c_str(),
                width,
                std::max(textH, diameter),
                {
                    .ExtraPaddingX = extra,
                    .ExtraPaddingY = extra,
                    .HitInsetRight = padX + diameter,
                    .Filled = true,
                }
            );
            const float small = 6.0F * scale;
            const float group = 16.0F * scale;
            const char *targetCaption = Platform::Tr("Target");
            const char *hitsCaption = Platform::Tr("Hits");
            const char *lastCaption = Platform::Tr("Last");
            const std::string hits = std::to_string(hook.Hits);
            const char *last = hook.Last.empty() ? "—" : hook.Last.c_str();
            const float fixed =
                ImGui::CalcTextSize(targetCaption).x + small + group +
                ImGui::CalcTextSize(hitsCaption).x + small + ImGui::CalcTextSize(hits.c_str()).x + group +
                ImGui::CalcTextSize(lastCaption).x + small;
            const float targetBudget = std::max(Em(8.0F), (row.ContentMax.x - diameter - small - row.ContentMin.x - fixed) * 0.62F);
            const float lastBudget = std::max(Em(6.0F), row.ContentMax.x - diameter - small - row.ContentMin.x - fixed - targetBudget);
            const float textY = row.ContentMin.y + ((row.ContentMax.y - row.ContentMin.y - textH) * 0.5F);
            const ImU32 captionCol = ImGui::GetColorU32(HexColor(Colors::TEXT_MUTED));
            const ImU32 valueCol = ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY));
            ImDrawList *draw = ImGui::GetWindowDrawList();
            float x = row.ContentMin.x;
            draw->AddText(ImVec2(x, textY), captionCol, targetCaption);
            x += ImGui::CalcTextSize(targetCaption).x + small;
            ClippedText(draw, ImVec2(x, textY), targetBudget, valueCol, hook.Target.c_str());
            x += targetBudget + group;
            draw->AddText(ImVec2(x, textY), captionCol, hitsCaption);
            x += ImGui::CalcTextSize(hitsCaption).x + small;
            draw->AddText(ImVec2(x, textY), valueCol, hits.c_str());
            x += ImGui::CalcTextSize(hits.c_str()).x + group;
            draw->AddText(ImVec2(x, textY), captionCol, lastCaption);
            x += ImGui::CalcTextSize(lastCaption).x + small;
            ClippedText(draw, ImVec2(x, textY), lastBudget, valueCol, last);

            ImGui::SameLine(0.0F, 0.0F);
            const float buttonY = row.Min.y + (((row.Max.y - row.Min.y) - diameter) * 0.5F);
            const ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            if (buttonY != buttonPos.y) {
                ImGui::SetCursorScreenPos(ImVec2(buttonPos.x, buttonY));
            }
            ImGui::PushID(static_cast<int>(hook.Id));
            const bool remove = IconCircleButton(
                "##remove",
                Icons::TIMES,
                true,
                {
                    .HoverFill = Colors::NEGATIVE,
                    .HoverGlyph = Colors::WHITE,
                },
                Platform::Tr("Remove")
            );
            ImGui::PopID();
            return remove;
        }

        void HookList(Context &context, FridaDeviceSession &session) {
            std::uint64_t removeId = 0;
            bool remove = false;
            const float height = std::max(Em(8.0F), ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("##FridaHookList", ImVec2(0.0F, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            if (session.Hooks.empty()) {
                CenteredMessage(Platform::Tr("No Hooks Available"), Platform::Tr("Install a Java or native hook to see it here."));
            }
            for (const FridaHookRow &hook: session.Hooks) {
                if (HookRow(hook)) {
                    remove = true;
                    removeId = hook.Id;
                }
            }
            ImGui::EndChild();
            if (remove) {
                RequestUnhook(context, session, removeId);
            }
        }
    }

    void BuildFridaHooksTab(Context &context, FridaDeviceSession &session) {
        HookToolbar(context, session);
        SpaceBelowToolbar();
        HookList(context, session);
    }
}

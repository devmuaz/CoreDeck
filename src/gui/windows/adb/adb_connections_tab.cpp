//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_connections_tab.h"

#include "adb_session.h"

#include "platform/i18n.h"

namespace CoreDeck::Gui::AdbWindow {
    namespace {
        const char *RouteArrow(const bool reverse) {
            return reverse ? "\xe2\x86\x90" : "\xe2\x86\x92";
        }

        void FormArrow(const char *arrow) {
            const float width = ImGui::CalcTextSize(arrow != nullptr ? arrow : "").x;
            const float height = ImGui::GetFrameHeight();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(std::max(1.0F, width), height));
            const ImVec2 text = ImGui::CalcTextSize(arrow != nullptr ? arrow : "");
            ImDrawList *draw = ImGui::GetWindowDrawList();
            draw->AddText(
                ImVec2(origin.x, origin.y + ((height - text.y) * 0.5F)),
                ImGui::GetColorU32(HexColor(Colors::TEXT_MUTED)),
                arrow != nullptr ? arrow : ""
            );
        }

        float RouteValueBudget(const float room, const float preferred, const float other) {
            if (preferred + other <= room) {
                return preferred;
            }
            const float half = room * 0.5F;
            if (preferred <= half) {
                return preferred;
            }
            if (other <= half) {
                return std::max(0.0F, room - other);
            }
            return std::max(0.0F, half);
        }

        float RouteField(
            ImDrawList *draw,
            const float x,
            const float y,
            const float valueBudget,
            const char *caption,
            const char *value,
            const float gap,
            const ImU32 captionColor,
            const ImU32 valueColor
        ) {
            const char *shownCaption = caption != nullptr ? caption : "";
            const char *shownValue = value != nullptr ? value : "";
            const float captionW = ImGui::CalcTextSize(shownCaption).x;
            draw->AddText(ImVec2(x, y), captionColor, shownCaption);
            const float valueX = x + captionW + gap;
            const float budget = std::max(0.0F, valueBudget);
            ClippedText(draw, ImVec2(valueX, y), budget, valueColor, shownValue);
            return valueX + std::min(ImGui::CalcTextSize(shownValue).x, budget);
        }

        bool PortRouteRow(const char *direction, const bool reverse, const char *host, const char *device, const bool enabled) {
            const float scale = GetDpiScale();
            const float extra = 4.0F * scale;
            const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float diameter = IconCircleButtonDiameter();
            const float textH = ImGui::GetTextLineHeight();
            const float padX = ImGui::GetStyle().FramePadding.x + extra;
            const DashboardRowResult row = DashboardRow(
                "##route",
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
            const char *directionCaption = Platform::Tr("Direction");
            const char *localCaption = Platform::Tr("Local");
            const char *remoteCaption = Platform::Tr("Remote");
            const char *directionText = direction != nullptr ? direction : "";
            const char *hostText = host != nullptr ? host : "";
            const char *deviceText = device != nullptr ? device : "";
            const char *arrow = RouteArrow(reverse);
            const float arrowW = ImGui::CalcTextSize(arrow).x;
            const float directionW = ImGui::CalcTextSize(directionText).x;
            const float hostW = ImGui::CalcTextSize(hostText).x;
            const float deviceW = ImGui::CalcTextSize(deviceText).x;
            const float fixed =
                ImGui::CalcTextSize(directionCaption).x + small + directionW + group +
                ImGui::CalcTextSize(localCaption).x + small + group + arrowW + group +
                ImGui::CalcTextSize(remoteCaption).x + small;
            const float room = std::max(0.0F, row.ContentMax.x - diameter - small - row.ContentMin.x - fixed);
            const float localBudget = RouteValueBudget(room, hostW, deviceW);
            const float remoteBudget = RouteValueBudget(room, deviceW, hostW);
            const float textY = row.ContentMin.y + ((row.ContentMax.y - row.ContentMin.y - textH) * 0.5F);
            const ImU32 captionCol = ImGui::GetColorU32(HexColor(Colors::TEXT_MUTED));
            const ImU32 valueCol = ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY));
            ImDrawList *draw = ImGui::GetWindowDrawList();
            float x = RouteField(draw, row.ContentMin.x, textY, directionW, directionCaption, directionText, small, captionCol, valueCol);
            x = RouteField(draw, x + group, textY, localBudget, localCaption, hostText, small, captionCol, valueCol);
            draw->AddText(ImVec2(x + group, textY), captionCol, arrow);
            RouteField(draw, x + group + arrowW + group, textY, remoteBudget, remoteCaption, deviceText, small, captionCol, valueCol);

            ImGui::SameLine(0.0F, 0.0F);
            const float buttonY = row.Min.y + (((row.Max.y - row.Min.y) - diameter) * 0.5F);
            const ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            if (buttonY != buttonPos.y) {
                ImGui::SetCursorScreenPos(ImVec2(buttonPos.x, buttonY));
            }
            return IconCircleButton(
                "##remove",
                Icons::TIMES,
                enabled,
                {
                    .HoverFill = Colors::NEGATIVE,
                    .HoverGlyph = Colors::WHITE,
                },
                Platform::Tr("Remove")
            );
        }

        void RemovePortRoute(Context &context, AdbDeviceSession &session, const Core::AdbForward &item) {
            const std::string endpoint = item.Local;
            const bool reverse = item.Reverse;
            StartTask(
                context,
                session,
                reverse ? AdbTask::RemoveReverse : AdbTask::RemoveForward,
                [endpoint, reverse](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                    return RunPortCommand(sdk, serial, reverse, {"--remove", endpoint}, cancel);
                }
            );
        }

        void PortRoutesHeading(Context &context, AdbDeviceSession &session, const bool usable) {
            SectionHeading(Platform::Tr("Port routes"));
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float icon = IconCircleButtonDiameter();
            ImGui::SameLine(0.0F, spacing);
            const float avail = ImGui::GetContentRegionAvail().x;
            if (avail > icon) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - icon);
            }
            if (IconCircleButton(
                    "##RefreshRoutes",
                    Icons::REFRESH,
                    usable && !session.Busy.load(),
                    IconCircleButtonStyle(),
                    Platform::Tr("Refresh")
                )) {
                QueueListForwards(context, session);
            }
        }

        void PortRouteForm(Context &context, AdbDeviceSession &session, const bool usable) {
            int direction = session.ReverseForward ? 1 : 0;
            const char *directions[] = {
                Platform::Tr("Forward"),
                Platform::Tr("Reverse"),
            };
            if (CategoryChipRow(directions, IM_ARRAYSIZE(directions), direction)) {
                session.ReverseForward = direction == 1;
            }

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const bool reverse = session.ReverseForward;
            const char *arrow = RouteArrow(reverse);
            const float arrowW = ImGui::CalcTextSize(arrow).x;
            const char *add = Platform::Tr("Add Route");
            const float addPadX = 16.0F * GetDpiScale();
            const float addW = ImGui::CalcTextSize(add).x + ((ImGui::GetStyle().FramePadding.x + addPadX) * 2.0F);
            const float rowW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float fieldW = std::max(1.0F, (rowW - arrowW - addW - (spacing * 3.0F) - 2.0F) * 0.5F);
            const bool mappingValid = Core::IsValidAdbSocketSpec(session.ForwardLocal) &&
                                      Core::IsValidAdbSocketSpec(session.ForwardRemote);

            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint(
                "##ForwardLocal",
                Platform::Tr("Local endpoint, for example tcp:8080"),
                session.ForwardLocal,
                sizeof(session.ForwardLocal)
            );
            ImGui::SameLine(0.0F, spacing);
            FormArrow(arrow);
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint(
                "##ForwardRemote",
                Platform::Tr("Remote endpoint, for example tcp:8080"),
                session.ForwardRemote,
                sizeof(session.ForwardRemote)
            );
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(add, usable && !session.Busy.load() && mappingValid, ImVec2(addW, 0.0F))) {
                const std::string host = session.ForwardLocal;
                const std::string device = session.ForwardRemote;
                StartTask(
                    context,
                    session,
                    reverse ? AdbTask::AddReverse : AdbTask::AddForward,
                    [host, device, reverse](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                        if (reverse) {
                            return RunPortCommand(sdk, serial, true, {device, host}, cancel);
                        }
                        return RunPortCommand(sdk, serial, false, {host, device}, cancel);
                    }
                );
            }
        }

    }

    void ConnectionsTab(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &workspace = context.AdbWork;
        auto &session = SelectedSession(workspace);
        const Core::AdbDevice *device = SelectedDevice(snapshot, workspace.SelectedSerial);
        const bool usable = DeviceUsable(device);
        const float portHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        DetailCard("##PortMapping", portHeight, [&] {
            PortRoutesHeading(context, session, usable);
            PortRouteForm(context, session, usable);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("%s", Platform::Tr("Current routes"));
            ImGui::Spacing();
            const float rest = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("##PortRoutes", ImVec2(0.0F, rest), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            if (session.Forwards.empty() && session.Reverses.empty()) {
                CenteredMessage(
                    Platform::Tr("No Port Routes"),
                    Platform::Tr("Add a forward or reverse route to see it here.")
                );
            } else {
                const bool enabled = !session.Busy.load();
                int index = 0;
                const auto show = [&](const Core::AdbForward &item) {
                    ImGui::PushID(index);
                    ++index;
                    const char *direction = item.Reverse ? Platform::Tr("Reverse") : Platform::Tr("Forward");
                    const char *host = item.Reverse ? item.Remote.c_str() : item.Local.c_str();
                    const char *device = item.Reverse ? item.Local.c_str() : item.Remote.c_str();
                    if (PortRouteRow(direction, item.Reverse, host, device, enabled)) {
                        RemovePortRoute(context, session, item);
                    }
                    ImGui::PopID();
                };
                for (const Core::AdbForward &item: session.Forwards) {
                    show(item);
                }
                for (const Core::AdbForward &item: session.Reverses) {
                    show(item);
                }
            }
            ImGui::EndChild();
        });
    }
}

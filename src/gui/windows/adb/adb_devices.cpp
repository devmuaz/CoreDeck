//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_devices.h"

#include <charconv>
#include <optional>

#include "adb_session.h"
#include "adb_wifi_help.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        const AdbDeviceSession *SessionFor(const AdbWork &workspace, const std::string &serial) {
            const auto found = workspace.Sessions.find(serial);
            if (found == workspace.Sessions.end() || found->second == nullptr) {
                return nullptr;
            }
            return found->second.get();
        }

        bool SessionBusy(const AdbWork &workspace, const std::string &serial) {
            const AdbDeviceSession *session = SessionFor(workspace, serial);
            return session != nullptr && session->Busy.load();
        }

        bool EndpointOnline(const Core::AdbDeviceSnapshot &snapshot, const std::string &endpoint) {
            const Core::AdbDevice *device = AdbWindow::SelectedDevice(snapshot, endpoint);
            return device != nullptr && device->State == Core::AdbDeviceState::Device;
        }

        bool IsPhysicalUsb(const Core::AdbDevice &device) {
            if (device.State != Core::AdbDeviceState::Device || device.EmulatorConsolePort > 0) {
                return false;
            }
            if (device.Serial.starts_with("emulator-") || Core::IsValidAdbNetworkEndpoint(device.Serial)) {
                return false;
            }
            return !device.Serial.empty();
        }

        std::string WifiConnectBlockedReason(const Core::AdbDevice *selected) {
            if (selected != nullptr && Core::IsValidAdbNetworkEndpoint(selected->Serial)) {
                return Platform::TrFormat("{0} is already connected over Wi-Fi.", AdbWindow::DeviceTitle(*selected));
            }
            if (selected != nullptr && (selected->EmulatorConsolePort > 0 || selected->Serial.starts_with("emulator-"))) {
                return Platform::Tr("Emulators do not use this Wi-Fi switch.");
            }
            return Platform::Tr("Select a USB device that is online and authorized.");
        }

        void BusyTooltip(const bool enabled, const bool busy) {
            if (!enabled && busy && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", Platform::Tr("Wait for the current command to finish."));
            }
        }

        void QueueConnect(Context &context, const std::string &endpoint) {
            AdbWindow::StartTask(
                context,
                context.AdbWork.Session(endpoint),
                AdbTask::Connect,
                [endpoint](const Core::SdkInfo &sdk, const std::string &, const std::atomic<bool> *cancel) {
                    return Core::RunAdb(sdk, {"connect", endpoint}, {}, cancel);
                }
            );
        }

        void QueueDisconnect(Context &context, const std::string &endpoint) {
            AdbWindow::StartTask(
                context,
                context.AdbWork.Session(endpoint),
                AdbTask::Disconnect,
                [endpoint](const Core::SdkInfo &sdk, const std::string &, const std::atomic<bool> *cancel) {
                    return Core::RunAdb(sdk, {"disconnect", endpoint}, {}, cancel);
                }
            );
        }

        int DigitsOnly(ImGuiInputTextCallbackData *data) { // NOLINT(misc-const-correctness)
            if (data->EventChar < '0' || data->EventChar > '9') {
                return 1;
            }
            return 0;
        }

        std::optional<int> WifiListenPort(const char *text) {
            if (text == nullptr || text[0] == '\0') {
                return std::nullopt;
            }
            int port = 0;
            const char *end = text;
            while (*end != '\0') {
                ++end;
            }
            const auto [ptr, error] = std::from_chars(text, end, port);
            if (error != std::errc{} || ptr != end || port < 1 || port > 65535) {
                return std::nullopt;
            }
            return port;
        }

        void QueueWifiConnect(Context &context, const std::string &serial, const int port) {
            AdbWindow::StartTask(
                context,
                context.AdbWork.Session(serial),
                AdbTask::ConnectOverWifi,
                [port](const Core::SdkInfo &sdk, const std::string &deviceSerial, const std::atomic<bool> *cancel) {
                    return Core::ConnectAdbDeviceOverWifi(sdk, deviceSerial, port, cancel);
                }
            );
        }

        void MutedWrapped(const char *text) {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::TextWrapped("%s", text != nullptr ? text : "");
        }

        float WidestWirelessAddress(const AdbWork &workspace) {
            float widest = 0.0F;
            for (const std::string &endpoint: workspace.WirelessDevices) {
                widest = std::max(widest, ImGui::CalcTextSize(endpoint.c_str()).x);
            }
            return widest;
        }

        float WirelessContentSpan(const AdbWork &workspace) {
            const float group = 16.0F * GetDpiScale();
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float statusW = std::max(
                ImGui::CalcTextSize(Platform::Tr("Online")).x,
                ImGui::CalcTextSize(Platform::Tr("Offline")).x
            );
            const float connectW = AdbWindow::ActionWidth(Platform::Tr("Connect"));
            const float disconnectW = AdbWindow::ActionWidth(Platform::Tr("Disconnect"));
            const float buttonCluster = std::max(connectW, disconnectW) + spacing + IconCircleButtonDiameter();
            return WidestWirelessAddress(workspace) + group + statusW + group + buttonCluster;
        }

        void SavedDeviceRow(Context &context, const Core::AdbDeviceSnapshot &snapshot, const std::string &endpoint) {
            const AdbWork &workspace = context.AdbWork;
            const bool online = EndpointOnline(snapshot, endpoint);
            const bool busy = SessionBusy(workspace, endpoint);
            const char *action = online ? Platform::Tr("Disconnect") : Platform::Tr("Connect");
            const float scale = GetDpiScale();
            const float extra = 4.0F * scale;
            const float group = 16.0F * scale;
            const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float diameter = IconCircleButtonDiameter();
            const float actionW = std::max(
                AdbWindow::ActionWidth(Platform::Tr("Connect")),
                AdbWindow::ActionWidth(Platform::Tr("Disconnect"))
            );
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float padX = ImGui::GetStyle().FramePadding.x + extra;
            const float textH = ImGui::GetTextLineHeight();
            const DashboardRowResult row = DashboardRow(
                "##device",
                width,
                std::max(textH, ImGui::GetFrameHeight()),
                {
                    .ExtraPaddingX = extra,
                    .ExtraPaddingY = extra,
                    .HitInsetRight = padX + actionW + spacing + diameter,
                    .Filled = true,
                }
            );

            const char *statusText = online ? Platform::Tr("Online") : Platform::Tr("Offline");
            const float textY = row.ContentMin.y + ((row.ContentMax.y - row.ContentMin.y - textH) * 0.5F);
            const float addressColumn = std::max(ImGui::CalcTextSize(endpoint.c_str()).x, WidestWirelessAddress(workspace));
            const float statusX = row.ContentMin.x + addressColumn + group;
            const ImU32 addressCol = ImGui::GetColorU32(HexColor(Colors::TEXT_PRIMARY));
            const ImU32 statusCol = ImGui::GetColorU32(HexColor(online ? Colors::POSITIVE : Colors::WARNING));
            ImDrawList *draw = ImGui::GetWindowDrawList();
            ClippedText(draw, ImVec2(row.ContentMin.x, textY), addressColumn, addressCol, endpoint.c_str());
            draw->AddText(ImVec2(statusX, textY), statusCol, statusText);

            const float frame = ImGui::GetFrameHeight();
            const float actionX = row.Max.x - padX - diameter - spacing - actionW;
            const float actionY = row.Min.y + (((row.Max.y - row.Min.y) - frame) * 0.5F);
            ImGui::SameLine(0.0F, 0.0F);
            ImGui::SetCursorScreenPos(ImVec2(actionX, actionY));
            const bool canAct = !busy;
            if (online) {
                if (NegativeButton(action, canAct, ImVec2(actionW, 0.0F))) {
                    QueueDisconnect(context, endpoint);
                }
            } else if (PrimaryButton(action, canAct, ImVec2(actionW, 0.0F))) {
                QueueConnect(context, endpoint);
            }
            BusyTooltip(canAct, busy);

            ImGui::SameLine(0.0F, spacing);
            const float iconY = row.Min.y + (((row.Max.y - row.Min.y) - diameter) * 0.5F);
            const ImVec2 iconPos = ImGui::GetCursorScreenPos();
            if (iconY != iconPos.y) {
                ImGui::SetCursorScreenPos(ImVec2(iconPos.x, iconY));
            }
            if (IconCircleButton(
                    "##Remove",
                    Icons::TIMES,
                    !busy,
                    {
                        .HoverFill = Colors::NEGATIVE,
                        .HoverGlyph = Colors::WHITE,
                    },
                    Platform::Tr("Remove")
                )) {
                AdbWindow::ForgetWirelessDevice(context, endpoint);
            }
        }

        void WifiConnectRow(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            auto &workspace = context.AdbWork;
            const Core::AdbDevice *selected = AdbWindow::SelectedDevice(snapshot, workspace.SelectedSerial);
            const bool overUsb = selected != nullptr && IsPhysicalUsb(*selected);
            const std::optional<int> port = WifiListenPort(workspace.WifiPort);
            const std::string label =
                overUsb
                    ? Platform::TrFormat("Connect {0} over Wi-Fi", AdbWindow::DeviceTitle(*selected))
                    : Platform::Tr("Connect over Wi-Fi");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonW = AdbWindow::ActionWidth(label.c_str());
            const float portW = ImGui::CalcTextSize("65535").x + (ImGui::GetStyle().FramePadding.x * 2.0F);
            const float icon = IconCircleButtonDiameter();
            const bool busy = selected != nullptr && SessionBusy(workspace, selected->Serial);
            const bool portValid = port.has_value();
            const bool enabled = overUsb && !busy && portValid;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Platform::Tr("Port"));
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(portW);
            ImGui::InputText(
                "##WifiPort",
                workspace.WifiPort,
                sizeof(workspace.WifiPort),
                ImGuiInputTextFlags_CallbackCharFilter,
                DigitsOnly
            );
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(label.c_str(), enabled, ImVec2(buttonW, 0.0F)) && selected != nullptr && portValid) {
                QueueWifiConnect(context, selected->Serial, *port);
            }
            if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                std::string reason = WifiConnectBlockedReason(selected);
                if (overUsb && busy) {
                    reason = Platform::Tr("Wait for the current command to finish.");
                } else if (overUsb && !portValid) {
                    reason = Platform::Tr("Enter a port from 1 to 65535.");
                }
                ImGui::SetTooltip("%s", reason.c_str());
            }

            ImGui::SameLine(0.0F, spacing);
            const float frame = ImGui::GetFrameHeight();
            const float y = ImGui::GetCursorPosY();
            if (frame > icon) {
                ImGui::SetCursorPosY(y + ((frame - icon) * 0.5F));
            }
            if (IconCircleButton("##WifiConnectHelp", Icons::QUESTION, true, IconCircleButtonStyle(), Platform::Tr("How this works"))) {
                workspace.ShowWifiConnectHelp = true;
            }
        }

        void AddDeviceRow(Context &context) {
            auto &workspace = context.AdbWork;
            const char *connect = Platform::Tr("Connect");
            const float connectW = AdbWindow::ActionWidth(connect);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float fieldW = std::max(1.0F, ImGui::GetContentRegionAvail().x - connectW - spacing);
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputTextWithHint(
                "##WirelessAddress",
                Platform::Tr("Host:port"),
                workspace.WirelessAddress,
                sizeof(workspace.WirelessAddress)
            );
            const std::string address = workspace.WirelessAddress;
            const bool valid = Core::IsValidAdbNetworkEndpoint(address);
            const bool busy = SessionBusy(workspace, address);
            ImGui::SameLine(0.0F, spacing);
            const bool enabled = valid && !busy;
            if (PrimaryButton(connect, enabled, ImVec2(connectW, 0.0F))) {
                AdbWindow::RememberWirelessDevice(context, address);
                QueueConnect(context, address);
            }
            BusyTooltip(enabled, busy);
        }

        void SavedDevices(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            const std::vector<std::string> saved = context.AdbWork.WirelessDevices;
            if (saved.empty()) {
                ImGui::TextDisabled("%s", Platform::Tr("No saved devices yet."));
                return;
            }
            for (int index = 0; std::cmp_less(index, saved.size()); ++index) {
                ImGui::PushID(index);
                SavedDeviceRow(context, snapshot, saved.at(static_cast<std::size_t>(index)));
                ImGui::PopID();
            }
        }

        void WifiCard(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            SectionHeading(Platform::Tr("Connect over Wi-Fi"));
            MutedWrapped(Platform::Tr("Switches the USB device selected in the list to Wi-Fi."));
            WifiConnectRow(context, snapshot);
        }

        void DevicesCard(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
            SectionHeading(Platform::Tr("Devices"));
            AddDeviceRow(context);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("%s", Platform::Tr("Saved devices"));
            ImGui::Spacing();
            const float rest = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("##SavedDevices", ImVec2(0.0F, rest), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            SavedDevices(context, snapshot);
            ImGui::EndChild();
        }
    }

    void BuildDevicesWindow(Context &context, const Core::AdbDeviceSnapshot &snapshot) {
        auto &workspace = context.AdbWork;
        const std::string title = Platform::TrWindow("Wireless Devices", "AdbDevices");
        static bool windowWasOpen = false;
        if (!workspace.ShowDevicesWindow && !ImGui::IsPopupOpen(title.c_str())) {
            workspace.ShowWifiConnectHelp = false;
            if (windowWasOpen && workspace.DevicesTone != StatusBarTone::Busy) {
                workspace.DevicesTone = StatusBarTone::Idle;
                workspace.DevicesMessage.clear();
            }
            windowWasOpen = false;
            return;
        }
        windowWasOpen = true;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        const ImGuiStyle &style = ImGui::GetStyle();
        const char *blurb = Platform::Tr("Switches the USB device selected in the list to Wi-Fi.");
        const float chromeX = (style.WindowPadding.x * 2.0F) + (style.WindowBorderSize * 2.0F);
        const float rowPad = style.FramePadding.x + (4.0F * GetDpiScale());
        const float rowWindow =
            WirelessContentSpan(workspace) +
            (rowPad * 2.0F) +
            (style.WindowPadding.x * 4.0F) +
            (style.WindowBorderSize * 2.0F) +
            (style.ChildBorderSize * 2.0F) +
            style.ScrollbarSize +
            Em(2.0F);
        const float width = std::min(
            std::max(Em(40.0F), viewport->WorkSize.x * 0.92F),
            std::max(rowWindow, std::max(Em(60.0F), ImGui::CalcTextSize(blurb).x + chromeX + Em(12.0F)))
        );
        const float height = std::min(viewport->WorkSize.y * 0.84F, Eh(28.0F));
        constexpr ImGuiWindowFlags FLAGS = WINDOW_NO_RESIZE_FLAGS |
                                           ImGuiWindowFlags_NoScrollbar |
                                           ImGuiWindowFlags_NoScrollWithMouse;
        if (!BeginCenteredModal(title.c_str(), &workspace.ShowDevicesWindow, ImVec2(width, height), FLAGS)) {
            return;
        }

        const float gap = ImGui::GetStyle().ItemSpacing.y;
        const float statusH = StatusBarHeight();
        const float bodyH = std::max(1.0F, ImGui::GetContentRegionAvail().y - statusH - gap);
        {
            StyleColor sc;
            sc.Push(ImGuiCol_ChildBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
            ImGui::BeginChild("##WirelessBody", ImVec2(0.0F, bodyH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            const auto wifi = [&] {
                WifiCard(context, snapshot);
            };
            const float wifiH = AdbWindow::FittedCardHeight("##Wifi", wifi, 0.0F);
            DetailCard("##Wifi", wifiH, wifi);
            const float devicesH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            {
                StyleColor sc;
                StyleVar sv;
                PushDetailCardChrome(sc, sv);
                ImGui::BeginChild(
                    "##Devices",
                    ImVec2(0.0F, devicesH),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                );
                DevicesCard(context, snapshot);
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        std::string validation;
        if (workspace.WifiPort[0] != '\0' && !WifiListenPort(workspace.WifiPort).has_value()) {
            validation = Platform::Tr("Enter a port from 1 to 65535.");
        }
        if (workspace.WirelessAddress[0] != '\0' && !Core::IsValidAdbNetworkEndpoint(workspace.WirelessAddress)) {
            if (!validation.empty()) {
                validation.push_back(' ');
            }
            validation += Platform::Tr("Enter an address in host:port form.");
        }
        const char *status = Platform::Tr("No wireless action yet.");
        StatusBarTone tone = StatusBarTone::Idle;
        if (!validation.empty()) {
            status = validation.c_str();
            tone = StatusBarTone::Warning;
        } else if (!workspace.DevicesMessage.empty()) {
            status = workspace.DevicesMessage.c_str();
            tone = workspace.DevicesTone;
        }
        StatusBar(tone, status);
        if (workspace.ShowDevicesWindow) {
            WifiConnectHelp(workspace.ShowWifiConnectHelp);
        }
        ImGui::EndPopup();
    }
}

//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#include "adb_wifi_help.h"

#include <algorithm>
#include <string>

#include "imgui.h"

#include "../../widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        void MutedWrapped(const char *text) {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
            ImGui::TextWrapped("%s", text);
        }

        void CommandLines(const char *const *commands, const int count) {
            if (commands == nullptr || count <= 0) {
                return;
            }
            ImGui::Indent();
            {
                StyleColor sc;
                sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
                for (int index = 0; index < count; ++index) {
                    ImGui::TextWrapped("%s", commands[index]);
                }
            }
            ImGui::Unindent();
        }

        void WifiHelpBlock(
            const char *title,
            const char *detail,
            const char *const *commands,
            const int commandCount
        ) {
            ImGui::TextUnformatted(title);
            MutedWrapped(detail);
            CommandLines(commands, commandCount);
            ImGui::Spacing();
        }
    }

    void WifiConnectHelp(bool &open) {
        const std::string title = Platform::StrConcat(
            Platform::Tr("How Connect over Wi-Fi works"),
            "###WifiConnectHelp"
        );
        if (!open && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoSavedSettings;

        const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
        const float width = std::min(Em(88.0F), std::max(Em(36.0F), work.x * 0.92F));
        const float maxHeight = std::max(Eh(12.0F), work.y * 0.85F);
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0F), ImVec2(width, maxHeight));
        if (!BeginCenteredModal(title.c_str(), &open, ImVec2(width, 0.0F), FLAGS)) {
            return;
        }
        if (!open) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        MutedWrapped(Platform::Tr("This switches the selected USB phone to a wireless adb connection. The port field starts at 5555. The button stays off when that device is already a Wi-Fi address, an emulator, offline, or not authorized."));
        ImGui::Spacing();

        WifiHelpBlock(
            Platform::Tr("1. Start from the USB device"),
            Platform::Tr("Unlock the phone, accept this computer's USB debugging prompt, and join the same Wi-Fi network. CoreDeck does not check the network. Nothing runs until you press the button, and every command uses the device selected in the list."),
            nullptr,
            0
        );
        const char *probes[] = {
            "adb -s <serial> shell ip -f inet addr",
            "adb -s <serial> shell ip addr",
            "adb -s <serial> shell getprop dhcp.wlan0.ipaddress",
        };
        WifiHelpBlock(
            Platform::Tr("2. Read the Wi-Fi address"),
            Platform::Tr("These run in order. A failed probe is skipped. CoreDeck keeps the first private IPv4 address on a wlan interface: 10.x.x.x, 172.16-31.x.x, or 192.168.x.x. The last command is only a bare address, so that address is used when the earlier output is not an interface listing. If no Wi-Fi address is found, CoreDeck stops and leaves the USB connection alone."),
            probes,
            3
        );
        const char *tcpip[] = {"adb -s <serial> tcpip <port>"};
        WifiHelpBlock(
            Platform::Tr("3. Listen on the TCP port"),
            Platform::Tr("This restarts adbd on the port in the field. The USB debugging session drops while it restarts, so that serial can disappear from the device list. The cable can stay plugged in. The field starts at 5555. If this command fails, CoreDeck does not try to connect."),
            tcpip,
            1
        );
        const char *connect[] = {"adb connect <ip>:<port>"};
        WifiHelpBlock(
            Platform::Tr("4. Connect to that address"),
            Platform::Tr(R"(CoreDeck waits 800 ms, then connects. This command has no -s flag: it talks to the address, not the USB serial. Success is a report containing "connected to" or "already connected". Anything else waits 600 ms and tries again, up to five times.)"),
            connect,
            1
        );
        WifiHelpBlock(
            Platform::Tr("5. Select and save the address"),
            Platform::Tr("When the connection succeeds, CoreDeck selects <ip>:<port> and saves it in Wireless Devices. You can connect, disconnect, or remove it there later. No further command runs."),
            nullptr,
            0
        );
        ImGui::EndPopup();
    }
}

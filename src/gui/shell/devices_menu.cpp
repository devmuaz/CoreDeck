//
// Created by AbdulMuaz Aqeel on 20/11/2026.
//

#include "gui/shell/devices_menu.h"

#include <cstdint>
#include <vector>

#include "core/adb/adb_client.h"
#include "gui/theme/theme.h"
#include "gui/widgets/device_picker.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        enum class DeviceLink : uint8_t {
            Usb,
            Wifi,
            Emulator,
        };

        bool IsEmulatorDevice(const Core::AdbDevice &device) {
            return device.EmulatorConsolePort > 0 || device.Serial.starts_with("emulator-");
        }

        bool IsWifiDevice(const Core::AdbDevice &device) {
            if (Core::IsValidAdbNetworkEndpoint(device.Serial)) {
                return true;
            }
            return device.Serial.find("_adb-tls-") != std::string::npos;
        }

        DeviceLink LinkFor(const Core::AdbDevice &device) {
            if (IsEmulatorDevice(device)) {
                return DeviceLink::Emulator;
            }
            if (IsWifiDevice(device)) {
                return DeviceLink::Wifi;
            }
            return DeviceLink::Usb;
        }

        const char *LinkIcon(const DeviceLink link) {
            switch (link) {
                case DeviceLink::Wifi:
                    return Icons::WIFI;
                case DeviceLink::Emulator:
                    return Icons::LAPTOP_CODE;
                case DeviceLink::Usb:
                    return Icons::MOBILE;
            }
            return Icons::MOBILE;
        }

        bool DeviceOnline(const Core::AdbDevice &device) {
            return device.State == Core::AdbDeviceState::Device;
        }

        std::string DeviceRowLabel(const Core::AdbDevice &device) {
            return Platform::StrConcat(IconWithLabel(LinkIcon(LinkFor(device)), DeviceLabel(device).c_str()), "##", device.Serial);
        }

        std::string DeviceTrailingGap() {
            const float check = ImGui::GetFontSize() * 0.866F;
            const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
            const float width = check + gap;
            std::string slot;
            while (ImGui::CalcTextSize(slot.c_str()).x < width) {
                slot.push_back(' ');
            }
            return slot;
        }

        void PaintTrailingMarks(const bool online, const bool selected) {
            const ImGuiStyle &style = ImGui::GetStyle();
            const auto spacingX = static_cast<float>(static_cast<int>(style.ItemSpacing.x * 0.5F));
            const auto spacingY = static_cast<float>(static_cast<int>(style.ItemSpacing.y * 0.5F));
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float radius = ComboDotRadius();
            const float gap = style.ItemInnerSpacing.x;
            const float contentRight = max.x - (style.ItemSpacing.x - spacingX);
            const float top = min.y + spacingY;
            const float bottom = max.y - (style.ItemSpacing.y - spacingY);
            const float centerY = (top + bottom) * 0.5F;
            float cursorRight = contentRight - gap;
            ImDrawList *draw = ImGui::GetWindowDrawList();
            if (online) {
                const float centerX = cursorRight - radius;
                draw->AddCircleFilled(ImVec2(centerX, centerY), radius, ImGui::GetColorU32(HexColor(Colors::POSITIVE)));
                cursorRight = centerX - radius - gap;
            }
            if (selected) {
                const float checkSize = ImGui::GetFontSize() * 0.866F;
                ImGui::RenderCheckMark(
                    draw,
                    ImVec2(cursorRight - checkSize, centerY - (checkSize * 0.5F)),
                    ImGui::GetColorU32(ImGuiCol_Text),
                    checkSize
                );
            }
        }

        Core::AdbDeviceSnapshot ListedDevices(Context const &context) {
            if (context.Host.Sdk.AdbPath.empty()) {
                return {};
            }
            Core::AdbDeviceSnapshot snapshot = context.Host.Adb.Devices();
            for (Core::AdbDevice &device: snapshot.Devices) {
                if (device.EmulatorConsolePort > 0) {
                    device.AvdName = context.Host.Manager.GetAvdNameForConsolePort(device.EmulatorConsolePort);
                }
            }
            return snapshot;
        }

        std::vector<const Core::AdbDevice *> OrderedDevices(const Core::AdbDeviceSnapshot &snapshot) {
            std::vector<const Core::AdbDevice *> devices;
            devices.reserve(snapshot.Devices.size());
            const DeviceLink order[] = {DeviceLink::Usb, DeviceLink::Wifi, DeviceLink::Emulator};
            for (const DeviceLink link: order) {
                for (const Core::AdbDevice &device: snapshot.Devices) {
                    if (LinkFor(device) == link) {
                        devices.push_back(&device);
                    }
                }
            }
            return devices;
        }

        const std::string &ListedSelection(const Context &context) {
            return context.Workspaces.Adb.SelectedSerial;
        }

        void ChooseListedDevice(Context &context, const std::string &serial) {
            context.Workspaces.Adb.HeldSerial.clear();
            context.Workspaces.Adb.SelectedSerial = serial;
        }

        void DeviceNoneItem(const char *id) {
            const std::string label = Platform::StrConcat(Platform::Tr("No Available Devices"), "##", id);
            RoundedMenuItem(label.c_str(), nullptr, false, false);
        }

        void DevicesMenu(Context &context) {
            const Core::AdbDeviceSnapshot snapshot = ListedDevices(context);
            if (snapshot.Devices.empty()) {
                DeviceNoneItem("None");
                return;
            }
            const std::string &selected = ListedSelection(context);
            const std::string trailing = DeviceTrailingGap();
            for (const Core::AdbDevice *device: OrderedDevices(snapshot)) {
                const std::string label = DeviceRowLabel(*device);
                const bool current = device->Serial == selected;
                if (RoundedMenuItem(label.c_str(), trailing.c_str(), false)) {
                    ChooseListedDevice(context, device->Serial);
                }
                PaintTrailingMarks(DeviceOnline(*device), current);
            }
        }
    }

    void BuildDevicesMenu(Context &context) {
        if (!context.Host.Sdk.AdbPath.empty()) {
            context.Host.Adb.StartTracking();
        }
        if (!RoundedBeginMenu(Platform::Tr("Devices"))) {
            return;
        }
        DevicesMenu(context);
        ImGui::EndMenu();
    }
}

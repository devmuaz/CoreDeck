//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "gui/adb/adb_widgets.h"

#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "gui/adb/adb_controller.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::Adb {
    namespace {
        std::string DeviceOptionLabel(const Core::AdbDevice &device) {
            std::string title = DeviceTitle(device);
            if (title == device.Serial) {
                return title;
            }
            return title + " (" + device.Serial + ")";
        }
    }

    void DevicePicker(Context &context, const Core::AdbDeviceSnapshot &snapshot, const float room) {
        auto &work = context.Workspaces.Adb;
        const std::string manageLabel = IconWithLabel(Icons::WIFI, Platform::Tr("Manage Wireless Devices"));
        const float buttonW = ImGui::CalcTextSize(manageLabel.c_str()).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float right = ImGui::GetWindowContentRegionMax().x;
        const float comboRoom = std::max(1.0F, room - buttonW - spacing);
        const bool empty = snapshot.Devices.empty();

        float itemWidth = std::min(Em(24.0F), comboRoom);
        float comboWidth = itemWidth;
        std::vector<std::string> options;
        std::vector<ImVec4> dots;
        std::vector<const char *> labels;
        int selected = -1;
        std::string preview;
        const ImVec4 missingDot = HexColor(Colors::NEGATIVE);
        const ImVec4 *previewDot = nullptr;
        if (!empty) {
            options.reserve(snapshot.Devices.size());
            dots.reserve(snapshot.Devices.size());
            for (const Core::AdbDevice &device: snapshot.Devices) {
                options.push_back(DeviceOptionLabel(device));
                dots.push_back(HexColor(DeviceUsable(&device) ? Colors::POSITIVE : Colors::NEGATIVE));
            }
            labels.reserve(options.size());
            for (int index = 0; std::cmp_less(index, options.size()); ++index) {
                const auto deviceIndex = static_cast<std::size_t>(index);
                labels.push_back(options.at(deviceIndex).c_str());
                if (snapshot.Devices.at(deviceIndex).Serial == work.SelectedSerial) {
                    selected = index;
                }
            }
            preview = work.SelectedSerial;
            if (selected >= 0) {
                const auto deviceIndex = static_cast<std::size_t>(selected);
                preview = options.at(deviceIndex);
                previewDot = &dots.at(deviceIndex);
            } else if (work.SelectedSerial.empty()) {
                preview = Platform::Tr("Select a device");
            } else {
                previewDot = &missingDot;
            }
            const char *previewText = preview.c_str();
            const float slot = ComboDotSlot();
            comboWidth = std::max(
                ComboWidth(labels.data(), static_cast<int>(labels.size()), Em(24.0F), Em(48.0F)),
                ComboWidth(&previewText, 1, Em(24.0F), Em(48.0F))
            );
            itemWidth = comboWidth + slot;
            if (itemWidth > comboRoom) {
                itemWidth = std::max(1.0F, comboRoom);
                comboWidth = std::max(1.0F, itemWidth - slot);
            }
        }

        const float groupW = buttonW + spacing + itemWidth;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - groupW));
        if (PickerButton(manageLabel.c_str(), true, ImVec2(buttonW, 0.0F))) {
            work.ShowDevicesWindow = true;
        }
        ImGui::SameLine(0.0F, spacing);
        if (empty) {
            DisabledCombo("##AdbDevice", Platform::Tr("No devices"), itemWidth);
            return;
        }
        if (RoundedCombo(
                "##AdbDevice",
                preview.c_str(),
                labels.data(),
                static_cast<int>(labels.size()),
                selected,
                comboWidth,
                nullptr,
                previewDot,
                dots.data()
            ) &&
            selected >= 0 && std::cmp_less(selected, snapshot.Devices.size())) {
            work.SelectedSerial = snapshot.Devices.at(static_cast<std::size_t>(selected)).Serial;
        }
    }

    void ShowTabStatus(
        const AdbState &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device,
        const std::optional<std::size_t> capturedLines
    ) {
        AdbTabStatus status = VisibleTabStatus(workspace, session, snapshot, device);
        if (capturedLines.has_value()) {
            status.Message = Platform::TrFormat("{0} [{1} lines]", status.Message, std::to_string(*capturedLines));
        }
        StatusBar(status.Tone, status.Message.c_str());
    }
}

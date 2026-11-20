//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/widgets/device_picker.h"

#include <algorithm>
#include <string>
#include <vector>

#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        bool DeviceReady(const Core::AdbDevice &device) {
            return device.State == Core::AdbDeviceState::Device || device.State == Core::AdbDeviceState::Recovery;
        }

        std::string DeviceOptionLabel(const Core::AdbDevice &device) {
            std::string title = device.Serial;
            if (!device.AvdName.empty()) {
                title = device.AvdName;
            } else if (!device.Model.empty()) {
                title = device.Model;
                std::ranges::replace(title, '_', ' ');
            }
            if (title == device.Serial) {
                return title;
            }
            return title + " (" + device.Serial + ")";
        }
    }

    void DevicePicker(
        const char *id,
        std::string &selectedSerial,
        const Core::AdbDeviceSnapshot &snapshot,
        const float room,
        const char *buttonLabel,
        bool *buttonPressed
    ) {
        const bool hasButton = buttonLabel != nullptr && buttonLabel[0] != '\0';
        const float buttonW = hasButton ? ImGui::CalcTextSize(buttonLabel).x + (ImGui::GetStyle().FramePadding.x * 2.0F) : 0.0F;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float right = ImGui::GetWindowContentRegionMax().x;
        const float comboRoom = std::max(1.0F, room - (hasButton ? buttonW + spacing : 0.0F));
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
                dots.push_back(HexColor(DeviceReady(device) ? Colors::POSITIVE : Colors::NEGATIVE));
            }
            labels.reserve(options.size());
            for (int index = 0; std::cmp_less(index, options.size()); ++index) {
                const auto deviceIndex = static_cast<std::size_t>(index);
                labels.push_back(options.at(deviceIndex).c_str());
                if (snapshot.Devices.at(deviceIndex).Serial == selectedSerial) {
                    selected = index;
                }
            }
            preview = selectedSerial;
            if (selected >= 0) {
                const auto deviceIndex = static_cast<std::size_t>(selected);
                preview = options.at(deviceIndex);
                previewDot = &dots.at(deviceIndex);
            } else if (selectedSerial.empty()) {
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

        const float groupW = buttonW + (hasButton ? spacing : 0.0F) + itemWidth;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - groupW));
        if (hasButton && PickerButton(buttonLabel, true, ImVec2(buttonW, 0.0F)) && buttonPressed != nullptr) {
            *buttonPressed = true;
        }
        if (hasButton) {
            ImGui::SameLine(0.0F, spacing);
        }
        if (empty) {
            DisabledCombo(id, Platform::Tr("No devices"), itemWidth);
            return;
        }
        if (RoundedCombo(id, preview.c_str(), labels.data(), static_cast<int>(labels.size()), selected, comboWidth, nullptr, previewDot, dots.data()) &&
            selected >= 0 && std::cmp_less(selected, snapshot.Devices.size())) {
            selectedSerial = snapshot.Devices.at(static_cast<std::size_t>(selected)).Serial;
        }
    }
}

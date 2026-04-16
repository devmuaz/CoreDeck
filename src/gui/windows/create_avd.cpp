//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include "imgui.h"

#include "create_avd.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    static int DigitsOnlyFilter(ImGuiInputTextCallbackData *data) {
        return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
    }

    static int AvdNameFilter(ImGuiInputTextCallbackData *data) {
        const ImWchar c = data->EventChar;
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        return ok ? 0 : 1;
    }

    static bool AvdNameExists(const std::vector<std::string> &names, const std::string &candidate) {
        if (candidate.empty()) return false;
        auto lower = [](std::string s) {
            std::ranges::transform(s, s.begin(), [](const unsigned char ch) { return std::tolower(ch); });
            return s;
        };
        const std::string needle = lower(candidate);
        return std::ranges::any_of(names, [&](const std::string &n) { return lower(n) == needle; });
    }

    void BuildCreateAvdWindow(Context &context) {
        if (context.ShowCreateDialog && !ImGui::IsPopupOpen("Create New AVD###CreateAvdDialog")) {
            ImGui::OpenPopup("Create New AVD###CreateAvdDialog");
        }

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::BeginPopupModal("Create New AVD###CreateAvdDialog", &context.ShowCreateDialog, flags)) {
            const bool isLoading = context.CreateDataLoading.load();
            const bool isCreating = context.AsyncBusy.load();
            const bool formDisabled = isLoading || isCreating;

            if (formDisabled) ImGui::BeginDisabled();

            const float nameRowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float nameColWidth = (ImGui::GetContentRegionAvail().x - nameRowSpacing) * 0.5f;
            const float nameCol2X = ImGui::GetCursorPosX() + nameColWidth + nameRowSpacing;

            ImGui::Text("AVD Name");
            ImGui::SameLine();
            ImGui::SetCursorPosX(nameCol2X);
            ImGui::Text("Display Name");

            char nameBuffer[128];
            strncpy(nameBuffer, context.CreateParams.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(nameColWidth);
            if (ImGui::InputTextWithHint("##AvdName", "e.g. MyPixel7", nameBuffer, sizeof(nameBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, AvdNameFilter)) {
                context.CreateParams.Name = nameBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(nameCol2X);

            char displayBuffer[128];
            strncpy(displayBuffer, context.CreateParams.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(nameColWidth);
            if (ImGui::InputTextWithHint("##DisplayName", "e.g. My Pixel 7", displayBuffer, sizeof(displayBuffer))) {
                context.CreateParams.DisplayName = displayBuffer;
            }

            if (AvdNameExists(context.AvdNames, context.CreateParams.Name)) {
                ImGui::TextColored(
                    HexColor("#E64D40"),
                    "An AVD named \"%s\" already exists.",
                    context.CreateParams.Name.c_str()
                );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("System Image");
            if (context.CreateDataReady && context.SystemImages

                .
                empty()
            ) {
                ImGui::TextColored(
                    HexColor("#E64D40"),
                    "No system images found. Install one via Android Studio SDK Manager."
                );
            } else if (!context.SystemImages.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##SystemImage",
                                      context.SystemImages[context.SelectedSystemImage].DisplayName.c_str())) {
                    for (int i = 0; i < static_cast<int>(context.SystemImages.size()); i++) {
                        const bool isSelected = context.SelectedSystemImage == i;
                        if (ImGui::Selectable(context.SystemImages[i].DisplayName.c_str(), isSelected)) {
                            context.SelectedSystemImage = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::BeginCombo("##SystemImage", "Loading...");
            }

            ImGui::Spacing();

            ImGui::Text("Device");
            if (context.CreateDataReady && context.DeviceProfiles
                .
                empty()
            ) {
                ImGui::TextColored(HexColor("#E64D40"), "No device profiles found.");
            } else if (!context.DeviceProfiles.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##device", context.DeviceProfiles[context.SelectedDevice].Name.c_str())) {
                    for (int i = 0; i < static_cast<int>(context.DeviceProfiles.size()); i++) {
                        const bool isSelected = context.SelectedDevice == i;
                        if (ImGui::Selectable(context.DeviceProfiles[i].Name.c_str(), isSelected)) {
                            context.SelectedDevice = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::BeginCombo("##device", "Loading...");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float rowSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float colWidth = (ImGui::GetContentRegionAvail().x - rowSpacing) * 0.5f;
            const float col2X = ImGui::GetCursorPosX() + colWidth + rowSpacing;

            ImGui::Text("RAM (MB)");
            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);
            ImGui::Text("SD Card Size");

            char ramBuffer[32];
            strncpy(ramBuffer, context.CreateParams.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##ram", "e.g. 2048 (MB)", ramBuffer, sizeof(ramBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.CreateParams.RamSize = ramBuffer;
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(col2X);

            char sdBuffer[32];
            strncpy(sdBuffer, context.CreateParams.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            ImGui::SetNextItemWidth(colWidth);
            if (ImGui::InputTextWithHint("##sdcard", "e.g. 512 (MB)", sdBuffer, sizeof(sdBuffer),
                                         ImGuiInputTextFlags_CallbackCharFilter, DigitsOnlyFilter)) {
                context.CreateParams.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("GPU Mode");
            static const char *gpuModes[] = {"auto", "host", "swiftshader_indirect", "guest"};
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##gpu", gpuModes[context.SelectedGpuMode])) {
                for (int i = 0; i < 4; i++) {
                    const bool isSelected = context.SelectedGpuMode == i;
                    if (ImGui::Selectable(gpuModes[i], isSelected)) {
                        context.SelectedGpuMode = i;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (formDisabled) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Spacing();

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            const bool canCreate = !context.CreateParams.Name.empty() && !context.SystemImages.empty()
                                   && !context.DeviceProfiles.empty() && !formDisabled;

            if (isCreating) {
                ImGui::BeginDisabled();
                PositiveButton("Creating...", false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                if (PositiveButton("Create", canCreate, ImVec2(halfWidth, 0))) {
                    context.CreateParams.SystemImagePackagePath = context.SystemImages[context.SelectedSystemImage].
                            PackagePath;
                    context.CreateParams.DeviceId = context.DeviceProfiles[context.SelectedDevice].Id;
                    context.CreateParams.GpuMode = gpuModes[context.SelectedGpuMode];
                    if (!context.CreateParams.SdCardSize.empty()) {
                        context.CreateParams.SdCardSize += "M";
                    }

                    context.AsyncBusy = true;
                    context.AsyncFuture = std::async(std::launch::async, [&context] {
                        CreateAvd(context.Sdk, context.CreateParams);
                        context.AsyncBusy = false;
                    });
                }
            }
            ImGui::SameLine();
            if (PrimaryButton("Cancel", !isCreating, ImVec2(halfWidth, 0))) {
                context.ShowCreateDialog = false;
                ImGui::CloseCurrentPopup();
            }

            // Check if async create finished
            if (!context.AsyncBusy && context.AsyncFuture.valid()) {
                context.AsyncFuture.get();
                context.ShowCreateDialog = false;
                ImGui::CloseCurrentPopup();
                RefreshAvds(context);
            }

            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }
}

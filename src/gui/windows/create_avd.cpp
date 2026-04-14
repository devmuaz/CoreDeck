//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "create_avd.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    void BuildCreateAvdWindow(Context &context) {
        if (!context.ShowCreateDialog) return;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Create New AVD###CreateAvdDialog", &context.ShowCreateDialog, flags)) {
            const bool isLoading = context.CreateDataLoading.load();
            const bool isCreating = context.AsyncBusy.load();
            const bool formDisabled = isLoading || isCreating;

            if (formDisabled) ImGui::BeginDisabled();

            ImGui::Text("AVD Name");
            ImGui::SetNextItemWidth(-1.0f);
            char nameBuffer[128];
            strncpy(nameBuffer, context.CreateParams.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##AvdName", "e.g. MyPixel7", nameBuffer, sizeof(nameBuffer))) {
                context.CreateParams.Name = nameBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("Display Name");
            ImGui::SetNextItemWidth(-1.0f);
            char displayBuffer[128];
            strncpy(displayBuffer, context.CreateParams.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##DisplayName", "e.g. My Pixel 7", displayBuffer, sizeof(displayBuffer))) {
                context.CreateParams.DisplayName = displayBuffer;
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

            ImGui::Text("RAM (MB)");
            ImGui::SetNextItemWidth(-1.0f);
            char ramBuffer[32];
            strncpy(ramBuffer, context.CreateParams.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##ram", "e.g. 2048", ramBuffer, sizeof(ramBuffer))) {
                context.CreateParams.RamSize = ramBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("SD Card Size");
            ImGui::SetNextItemWidth(-1.0f);
            char sdBuffer[32];
            strncpy(sdBuffer, context.CreateParams.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##sdcard", "e.g. 512M", sdBuffer, sizeof(sdBuffer))) {
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
            }

            // Check if async create finished
            if (!context.AsyncBusy && context.AsyncFuture.valid()) {
                context.AsyncFuture.get();
                context.ShowCreateDialog = false;
                RefreshAvds(context);
            }

            ImGui::Spacing();
        }
        ImGui::End();
    }
}

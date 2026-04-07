//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "application.h"
#include "components.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "utilities.h"

namespace Emu {
    Application::Application() : m_Sdk(DetectAndroidSdk()),
                                 m_Options(GetDefaultOptions()),
                                 m_Manager(m_Sdk) {
        m_RefreshAvds();
    }

    void Application::m_RefreshAvds() {
        m_AvdNames = ListAvailableAvds(m_Sdk);
        m_Avds = LoadAvds(m_AvdNames);
        m_SelectedAvd = -1;
    }

    void Application::Build() {
        const ImGuiID dockSpaceId = ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_NoUndocking);

        static bool firstLaunch = true;
        if (firstLaunch) {
            firstLaunch = false;

            ImGui::DockBuilderRemoveNode(dockSpaceId);
            ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockSpaceId, ImGui::GetMainViewport()->Size);

            ImGuiID topId, bottomId;
            ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Down, 0.40f, &bottomId, &topId);

            ImGuiID leftId, centerId;
            ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.25, &leftId, &centerId);

            ImGuiID middleId, rightId;
            ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.40f, &rightId, &middleId);

            ImGui::DockBuilderDockWindow("Options", leftId);
            ImGui::DockBuilderDockWindow("AVDs", middleId);
            ImGui::DockBuilderDockWindow("Details", rightId);
            ImGui::DockBuilderDockWindow("Output Log", bottomId);

            ImGui::DockBuilderFinish(dockSpaceId);

            auto configureNode = [](const ImGuiID id) {
                if (ImGuiDockNode *node = ImGui::DockBuilderGetNode(id)) {
                    node->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
                }
            };
            configureNode(leftId);
            configureNode(middleId);
            configureNode(rightId);
            configureNode(bottomId);
        }

        m_BuildOptionsPanel();
        m_BuildAvdListPanel();
        m_BuildAvdDetailsPanel();
        m_BuildLogPanel();

        m_Manager.Update();
    }

    void Application::m_BuildOptionsPanel() {
        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Options", nullptr, panelFlags);

        for (auto &[Flag, DisplayName, Description, IsBoolean, Enabled, Hint, ValueBuffer]
             : m_Options) {
            ImGui::PushID(Flag.c_str());

            ImGui::Checkbox(DisplayName.c_str(), &Enabled);

            ImGui::SameLine();
            ImGui::TextColored(HexColor("#66666B"), Icons::Info);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Description.c_str());
            }

            if (!IsBoolean && Enabled) {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##val", Hint, ValueBuffer, IM_ARRAYSIZE(ValueBuffer));
            }

            ImGui::PopID();
        }

        ImGui::End();
    }

    void Application::m_BuildAvdListPanel() {
        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("AVDs", nullptr, panelFlags);

        if (PrimaryButton(IconWithLabel(Icons::Refresh, "Refresh").c_str())) {
            m_RefreshAvds();
        }

        if (m_SelectedAvd >= 0) {
            const auto &avd = m_Avds[m_SelectedAvd];
            const bool isRunning = m_Manager.IsRunning(avd.Name);
            const auto args = BuildArgs(avd.Name, m_Options);

            ImGui::SameLine();
            if (isRunning) {
                if (NegativeButton(IconWithLabel(Icons::Stop, "Turn AVD Off").c_str())) {
                    m_Manager.Stop(avd.Name);
                }
            } else {
                if (PositiveButton(IconWithLabel(Icons::Play, "Run").c_str())) {
                    m_Manager.Launch(avd.Name, args);
                }
                ImGui::SameLine();
                if (WarningButton(IconWithLabel(Icons::Terminal, "Wipe & Run").c_str())) {
                    auto wipeArgs = args;
                    wipeArgs.emplace_back("-wipe-data");
                    m_Manager.Launch(avd.Name, wipeArgs);
                }
            }
        }

        ImGui::Separator();

        if (m_Avds.empty()) {
            ImGui::TextDisabled("No AVDs found");
            ImGui::End();
            return;
        }

        ImGui::BeginChild("AvdList", ImVec2(0, 0), ImGuiChildFlags_None);

        for (int i = 0; i < static_cast<int>(m_Avds.size()); i++) {
            const auto &avd = m_Avds[i];
            const bool isSelected = (m_SelectedAvd == i);
            const bool isRunning = m_Manager.IsRunning(avd.Name);

            ImGui::PushID(i);
            const char *avdStatusText = isRunning ? "Running..." : "Ready";
            const ImVec4 avdStatusColor = isRunning ? HexColor("#33CC47") : HexColor("#66666B");
            if (SelectableItem(avd.DisplayName.c_str(), isSelected, avdStatusText, avdStatusColor)) {
                m_SelectedAvd = i;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void Application::m_BuildAvdDetailsPanel() {
        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        if (m_SelectedAvd < 0) {
            ImGui::Begin("Details###Details", nullptr, panelFlags);
            ImGui::TextDisabled("Select an AVD to view details");
            ImGui::End();
            return;
        }

        const auto &[
            Name,
            DisplayName,
            Device,
            ApiLevel,
            Abi,
            SdCard,
            RamSize,
            ScreenResolution,
            GpuMode,
            Arch,
            Path
        ] = m_Avds[m_SelectedAvd];
        const bool isRunning = m_Manager.IsRunning(Name);
        const auto args = BuildArgs(Name, m_Options);

        std::string preview = m_Sdk.EmulatorPath;
        for (const auto &arg: args) preview += " " + arg;

        ImGui::Begin(std::format("{}###Details", DisplayName).c_str(), nullptr, panelFlags);

        PropertyTextWrapped("AVD Path", Path.c_str());
        ImGui::Spacing();
        PropertyTextWrapped("Command", preview.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!Device.empty()) PropertyText("Device", Device.c_str());
        if (!ApiLevel.empty()) PropertyText("API Level", ApiLevel.c_str());
        if (!Abi.empty()) PropertyText("ABI", Abi.c_str());
        if (!Arch.empty()) PropertyText("Arch", Arch.c_str());
        if (!RamSize.empty()) PropertyText("RAM", std::format("{} MB", RamSize).c_str());
        if (!ScreenResolution.empty()) PropertyText("Resolution", ScreenResolution.c_str());
        if (!SdCard.empty()) PropertyText("Storage", SdCard.c_str());
        if (!GpuMode.empty()) PropertyText("GPU Mode", GpuMode.c_str());

        ImGui::End();
    }

    void Application::m_BuildLogPanel() {
        ImGui::Begin("Output Log");

        std::shared_ptr<LogBuffer> log = nullptr;
        if (m_SelectedAvd >= 0) {
            log = m_Manager.GetLog(m_Avds[m_SelectedAvd].Name);
        }

        if (!log) ImGui::BeginDisabled();
        if (PrimaryButton(IconWithLabel(Icons::Trash, "Clear").c_str())) log->Clear();
        ImGui::SameLine();
        if (!log) ImGui::EndDisabled();

        ImGui::Checkbox("Auto-Scroll", &m_AutoScroll);
        ImGui::Separator();
        ImGui::BeginChild("LogContent", ImVec2(0, 0), ImGuiChildFlags_None);

        if (log) {
            for (const auto lines = log->GetLines(); const auto &line: lines) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (m_AutoScroll && log->HasNewContent()) {
                ImGui::SetScrollHereY(1.0f);
                log->ResetNewContentFlag();
            }
        } else {
            ImGui::TextDisabled("Select an AVD to view logs");
        }

        ImGui::EndChild();
        ImGui::End();
    }
}

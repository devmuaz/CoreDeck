//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#include "application.h"
#include "components.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "utilities.h"
#include <algorithm>

namespace CoreDeck {
    Application::Application() : m_Sdk(DetectAndroidSdk()), m_Manager(m_Sdk) {
        EnsureConfigDirectoryExists();
        m_RefreshAvds();
    }

    void Application::m_RefreshAvds() {
        m_AvdNames = ListAvailableAvds(m_Sdk);
        m_Avds = LoadAvds(m_AvdNames);

        for (const auto &avdName: m_AvdNames) m_LoadAvdOptions(avdName);

        if (!m_Avds.empty()) m_SelectedAvd = 0;
        else m_SelectedAvd = -1;
        m_PreviousSelectedAvd = -1;
    }

    void Application::Build() {
        if (m_SelectedAvd != m_PreviousSelectedAvd) {
            if (m_SelectedAvd >= 0 && m_SelectedAvd < m_Avds.size()) {
                const std::string &avdName = m_Avds[m_SelectedAvd].Name;
                if (!m_PerAvdOptions.contains(avdName)) {
                    m_LoadAvdOptions(avdName);
                }
            }
            m_PreviousSelectedAvd = m_SelectedAvd;
        }

        const ImGuiID dockSpaceId = ImGui::DockSpaceOverViewport(
            0,
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_NoUndocking
        );

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

        std::string panelTitle = "Options";
        if (m_SelectedAvd >= 0 && m_SelectedAvd < m_Avds.size()) {
            panelTitle = "Options - " + m_Avds[m_SelectedAvd].DisplayName;
        }

        ImGui::Begin(
            (panelTitle + "###Options").c_str(),
            nullptr,
            panelFlags
        );

        if (m_SelectedAvd < 0) {
            ImGui::TextDisabled("Select an AVD to configure options");
            ImGui::End();
            return;
        }

        auto &options = m_GetCurrentAvdOptions();
        bool optionsChanged = false;

        std::vector<std::string> categories;
        for (const auto &option: options) {
            if (std::ranges::find(categories, option.Category) == categories.end()) {
                categories.push_back(option.Category);
            }
        }

        for (const auto &category: categories) {
            if (CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(20.0f);

                for (auto &[Flag, DisplayName, Description, Enabled, Type, Category, Hint, TextInput, Items,
                         SelectedItem]
                     : options) {
                    if (category != Category) continue;

                    ImGui::PushID(Flag.c_str());

                    const bool wasEnabled = Enabled;
                    ImGui::Checkbox(DisplayName.c_str(), &Enabled);
                    if (wasEnabled != Enabled) optionsChanged = true;

                    ImGui::SameLine();
                    ImGui::TextColored(HexColor("#66666B"), Icons::Info);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Description.c_str());

                    if (Enabled) {
                        switch (Type) {
                            case OptionType::TextInput: {
                                ImGui::SetNextItemWidth(-1.0f);
                                char buffer[256];
                                strncpy(buffer, TextInput.c_str(), sizeof(buffer) - 1);
                                buffer[sizeof(buffer) - 1] = '\0';
                                if (ImGui::InputTextWithHint("##val", Hint.c_str(), buffer, sizeof(buffer))) {
                                    TextInput = buffer;
                                    optionsChanged = true;
                                }
                                break;
                            }

                            case OptionType::Selection: {
                                ImGui::SetNextItemWidth(-1.0f);
                                if (ImGui::BeginCombo("##selection", Items[SelectedItem].c_str())) {
                                    for (int i = 0; i < Items.size(); ++i) {
                                        const bool isSelected = SelectedItem == i;
                                        if (ImGui::Selectable(Items[i].c_str(), isSelected)) {
                                            SelectedItem = i;
                                            optionsChanged = true;
                                        }
                                        if (isSelected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }
                                break;
                            }

                            default:
                                break;
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::Unindent(20.0f);
            }
        }

        if (optionsChanged) m_SaveAvdOptions(m_Avds[m_SelectedAvd].Name);
        ImGui::End();
    }

    void Application::m_BuildAvdListPanel() {
        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("AVDs - Main###AVDs", nullptr, panelFlags);

        if (PrimaryButton(IconWithLabel(Icons::Refresh, "Refresh").c_str())) {
            m_RefreshAvds();
        }

        if (m_SelectedAvd >= 0) {
            const auto &avd = m_Avds[m_SelectedAvd];
            const bool isRunning = m_Manager.IsRunning(avd.Name);
            const auto args = BuildArgs(avd.Name, m_GetCurrentAvdOptions());

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
        const auto args = BuildArgs(Name, m_GetCurrentAvdOptions());

        std::string preview = m_Sdk.EmulatorPath;
        for (const auto &arg: args) preview += " " + arg;

        ImGui::Begin(
            ("Details - " + DisplayName + "###Details").c_str(),
            nullptr,
            panelFlags
        );

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
        if (!RamSize.empty()) PropertyText("RAM", (RamSize + " MB").c_str());
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

        ImGui::SameLine();
        constexpr float searchWidth = 300.0f;
        const float windowWidth = ImGui::GetWindowWidth();
        constexpr float rightPadding = 8.0f;
        ImGui::SetCursorPosX(windowWidth - searchWidth - rightPadding);
        ImGui::SetNextItemWidth(searchWidth);
        constexpr std::string searchHint = std::string{Icons::Search} + " Search logs...";

        std::string currentSearch;
        if (m_SelectedAvd >= 0) {
            const std::string &avdName = m_Avds[m_SelectedAvd].Name;
            currentSearch = m_PerAvdLogSearch[avdName];
        }

        char searchBuffer[256];
        strncpy(searchBuffer, currentSearch.c_str(), sizeof(searchBuffer) - 1);
        searchBuffer[sizeof(searchBuffer) - 1] = '\0';

        if (ImGui::InputTextWithHint("##search", searchHint.c_str(), searchBuffer, sizeof(searchBuffer))) {
            if (m_SelectedAvd >= 0) {
                const std::string &avdName = m_Avds[m_SelectedAvd].Name;
                m_PerAvdLogSearch[avdName] = searchBuffer;
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("LogContent", ImVec2(0, 0), ImGuiChildFlags_None);

        if (m_SelectedAvd < 0) {
            ImGui::TextDisabled("Select an AVD to view logs");
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        if (log) {
            const auto lines = log->GetLines();

            if (lines.empty()) {
                ImGui::TextDisabled("No available logs to view");
                ImGui::EndChild();
                ImGui::End();
                return;
            }

            std::string searchQuery;
            const std::string &avdName = m_Avds[m_SelectedAvd].Name;
            searchQuery = m_PerAvdLogSearch[avdName];
            const bool hasSearch = !searchQuery.empty();

            bool hasVisibleLines = false;
            for (const auto &line: lines) {
                // Filter lines based on search query (case-insensitive)
                if (hasSearch) {
                    std::string lowerLine = line;
                    std::string lowerQuery = searchQuery;
                    std::ranges::transform(lowerLine, lowerLine.begin(), ::tolower);
                    std::ranges::transform(lowerQuery, lowerQuery.begin(), ::tolower);

                    if (lowerLine.find(lowerQuery) == std::string::npos) {
                        continue;
                    }
                }

                ImGui::TextUnformatted(line.c_str());
                hasVisibleLines = true;
            }

            if (hasSearch && !hasVisibleLines) {
                ImGui::TextDisabled("No matching log entries found");
            }

            if (m_AutoScroll && log->HasNewContent() && !hasSearch) {
                ImGui::SetScrollHereY(1.0f);
                log->ResetNewContentFlag();
            }
        } else {
            const std::string &avdName = m_Avds[m_SelectedAvd].Name;
            ImGui::TextDisabled("Run the \"%s\" AVD to view logs", avdName.c_str());
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void Application::m_LoadAvdOptions(const std::string &avdName) {
        const std::string configPath = GetOptionsConfigPath(avdName);
        m_PerAvdOptions[avdName] = LoadOptionsFromFile(configPath);
    }

    void Application::m_SaveAvdOptions(const std::string &avdName) {
        if (!m_PerAvdOptions.contains(avdName)) return;

        const std::string configPath = GetOptionsConfigPath(avdName);
        SaveOptionsToFile(configPath, m_PerAvdOptions[avdName]);
    }

    std::vector<EmulatorOption> &Application::m_GetCurrentAvdOptions() {
        if (m_SelectedAvd >= 0 && m_SelectedAvd < m_Avds.size()) {
            const std::string &avdName = m_Avds[m_SelectedAvd].Name;

            if (!m_PerAvdOptions.contains(avdName)) m_LoadAvdOptions(avdName);
            return m_PerAvdOptions[avdName];
        }

        // Fallback
        static std::vector<EmulatorOption> defaultOptions = GetEmulatorOptions();
        return defaultOptions;
    }
}

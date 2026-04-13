//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "application.h"
#include "components.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "utilities.h"
#include <algorithm>

namespace CoreDeck {
    Application::Application() : m_Sdk(DetectAndroidSdk()), m_Manager(m_Sdk) {
        EnsureOptionsConfigDirectoryExists();
        m_RefreshAvds();
    }

    void Application::m_RefreshAvds() {
        m_AvdNames = ListAvdNames(m_Sdk);
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

            // Only build the default layout if ImGui has no saved layout
            if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr ||
                ImGui::DockBuilderGetNode(dockSpaceId)->ChildNodes[0] == nullptr) {
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
        }

        m_BuildMenuBar();
        m_BuildOptionsPanel();
        m_BuildAvdListPanel();
        m_BuildAvdDetailsPanel();
        m_BuildLogPanel();
        m_BuildAboutDialog();
        m_BuildDeleteDialog();
        m_BuildCreateDialog();

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
        ImGui::Begin("Available AVDs (Android Virtual Device)###AVDs", nullptr, panelFlags);

        if (PrimaryButton(Icons::Refresh)) m_RefreshAvds();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh the AVD list");

        ImGui::SameLine();


        if (m_SelectedAvd >= 0) {
            const auto &avd = m_Avds[m_SelectedAvd];
            const bool isRunning = m_Manager.IsRunning(avd.Name);
            const auto args = BuildArgs(avd.Name, m_GetCurrentAvdOptions());

            ImGui::SameLine();
            if (isRunning) {
                if (NegativeButton(IconWithLabel(Icons::Stop, "Stop").c_str())) {
                    m_Manager.Stop(avd.Name);
                }
            } else {
                if (PositiveButton(Icons::Play)) {
                    m_Manager.Launch(avd.Name, args);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run the selected AVD");
                ImGui::SameLine();
                if (WarningButton(IconWithLabel(Icons::Terminal, "Wipe & Run").c_str())) {
                    auto wipeArgs = args;
                    wipeArgs.emplace_back("-wipe-data");
                    m_Manager.Launch(avd.Name, wipeArgs);
                }
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
                ImGui::SameLine(0, 15.0f);
                if (NegativeButton(Icons::Trash)) {
                    m_ShowDeleteDialog = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete currently selected AVD");
            }

            if (isRunning) {
                ImGui::SameLine(0, 15.0f);
                ImGui::Text("-");
            }

            ImGui::SameLine();
            if (PositiveButton(Icons::Plus)) {
                m_CreateParams = {};
                m_SelectedSystemImage = 0;
                m_SelectedDevice = 0;
                m_SelectedGpuMode = 0;
                m_CreateDataReady = false;
                m_CreateDataLoading = true;
                m_ShowCreateDialog = true;

                m_CreateDataFuture = std::async(std::launch::async, [this]() {
                    auto images = ListSystemImages(m_Sdk);
                    auto devices = ListDeviceProfiles(m_Sdk);
                    m_SystemImages = std::move(images);
                    m_DeviceProfiles = std::move(devices);
                    m_CreateDataLoading = false;
                    m_CreateDataReady = true;
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new AVD");
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
        const std::string searchHint = std::string{Icons::Search} + " Search logs...";

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

    void Application::m_BuildMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem(IconWithLabel(Icons::Info, "About CoreDeck").c_str())) {
                    m_ShowAboutDialog = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void Application::m_BuildAboutDialog() {
        if (!m_ShowAboutDialog) return;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("About CoreDeck", &m_ShowAboutDialog, flags)) {
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            const float titleWidth = ImGui::CalcTextSize("CoreDeck").x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleWidth) * 0.5f);
            ImGui::TextColored(HexColor("#F2F2F2"), "CoreDeck");
            ImGui::PopFont();

            const std::string versionText = "Version " COREDECK_VERSION " (Build " COREDECK_BUILD_NUMBER ")";
            const float versionWidth = ImGui::CalcTextSize(versionText.c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - versionWidth) * 0.5f);
            ImGui::TextColored(HexColor("#66666B"), "%s", versionText.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto desc = COREDECK_DESCRIPTION;
            const float descWidth = ImGui::CalcTextSize(desc).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - descWidth) * 0.5f);
            ImGui::TextUnformatted(desc);

            ImGui::Spacing();
            ImGui::Spacing();

            PropertyText("Author", COREDECK_VENDOR);
            PropertyText("License", "MIT");
            if (PropertyText("GitHub", "github.com/devmuaz/CoreDeck", true)) {
                OpenUrl("https://github.com/devmuaz/CoreDeck");
            }
            PropertyText("Built with", "C++20, Dear ImGui, GLFW, OpenGL");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const std::string copyright = "Copyright " "\xC2\xA9" " " COREDECK_YEAR " " COREDECK_VENDOR;
            const float copyrightWidth = ImGui::CalcTextSize(copyright.c_str()).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - copyrightWidth) * 0.5f);
            ImGui::TextColored(HexColor("#66666B"), "%s", copyright.c_str());

            ImGui::Spacing();
        }
        ImGui::End();
    }

    void Application::m_BuildDeleteDialog() {
        if (m_SelectedAvd < 0 || m_SelectedAvd >= static_cast<int>(m_Avds.size())) return;

        if (m_ShowDeleteDialog && !m_AsyncBusy && m_AsyncFuture.valid()) {
            m_AsyncFuture.get();
            m_ShowDeleteDialog = false;
            m_RefreshAvds();
            return;
        }

        const auto &avdName = m_Avds[m_SelectedAvd].Name;
        const std::string title = "Delete \"" + avdName + "\"?";
        const bool isDeleting = m_AsyncBusy.load();
        const DialogResult result = SimpleDialog(
            {
                .Id = "Delete###DeleteAvdDialog",
                .isOpen = m_ShowDeleteDialog,
                .title = title.c_str(),
                .message = "This will permanently remove the AVD and all its data. This action cannot be undone.",
                .confirmButtonTitle = "Delete",
                .cancelButtonTitle = "Cancel",
                .busyButtonTitle = "Deleting...",
                .type = DialogType::Negative,
                .isBusy = isDeleting
            }
        );

        if (result == DialogResult::Confirmed) {
            m_AsyncBusy = true;
            const std::string deletableAvdName = avdName;
            m_AsyncFuture = std::async(std::launch::async, [this, deletableAvdName]() {
                DeleteAvd(m_Sdk, deletableAvdName);
                m_AsyncBusy = false;
            });
        }
    }

    void Application::m_BuildCreateDialog() {
        if (!m_ShowCreateDialog) return;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Create New AVD###CreateAvdDialog", &m_ShowCreateDialog, flags)) {
            const bool isLoading = m_CreateDataLoading.load();
            const bool isCreating = m_AsyncBusy.load();
            const bool formDisabled = isLoading || isCreating;

            if (formDisabled) ImGui::BeginDisabled();

            ImGui::Text("AVD Name");
            ImGui::SetNextItemWidth(-1.0f);
            char nameBuffer[128];
            strncpy(nameBuffer, m_CreateParams.Name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##AvdName", "e.g. MyPixel7", nameBuffer, sizeof(nameBuffer))) {
                m_CreateParams.Name = nameBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("Display Name");
            ImGui::SetNextItemWidth(-1.0f);
            char displayBuffer[128];
            strncpy(displayBuffer, m_CreateParams.DisplayName.c_str(), sizeof(displayBuffer) - 1);
            displayBuffer[sizeof(displayBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##DisplayName", "e.g. My Pixel 7", displayBuffer, sizeof(displayBuffer))) {
                m_CreateParams.DisplayName = displayBuffer;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("System Image");
            if (m_CreateDataReady && m_SystemImages.empty()) {
                ImGui::TextColored(
                    HexColor("#E64D40"),
                    "No system images found. Install one via Android Studio SDK Manager."
                );
            } else if (!m_SystemImages.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##SystemImage", m_SystemImages[m_SelectedSystemImage].DisplayName.c_str())) {
                    for (int i = 0; i < static_cast<int>(m_SystemImages.size()); i++) {
                        const bool isSelected = (m_SelectedSystemImage == i);
                        if (ImGui::Selectable(m_SystemImages[i].DisplayName.c_str(), isSelected)) {
                            m_SelectedSystemImage = i;
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
            if (m_CreateDataReady && m_DeviceProfiles.empty()) {
                ImGui::TextColored(HexColor("#E64D40"), "No device profiles found.");
            } else if (!m_DeviceProfiles.empty()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##device", m_DeviceProfiles[m_SelectedDevice].Name.c_str())) {
                    for (int i = 0; i < static_cast<int>(m_DeviceProfiles.size()); i++) {
                        const bool isSelected = (m_SelectedDevice == i);
                        if (ImGui::Selectable(m_DeviceProfiles[i].Name.c_str(), isSelected)) {
                            m_SelectedDevice = i;
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
            strncpy(ramBuffer, m_CreateParams.RamSize.c_str(), sizeof(ramBuffer) - 1);
            ramBuffer[sizeof(ramBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##ram", "e.g. 2048", ramBuffer, sizeof(ramBuffer))) {
                m_CreateParams.RamSize = ramBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("SD Card Size");
            ImGui::SetNextItemWidth(-1.0f);
            char sdBuffer[32];
            strncpy(sdBuffer, m_CreateParams.SdCardSize.c_str(), sizeof(sdBuffer) - 1);
            sdBuffer[sizeof(sdBuffer) - 1] = '\0';
            if (ImGui::InputTextWithHint("##sdcard", "e.g. 512M", sdBuffer, sizeof(sdBuffer))) {
                m_CreateParams.SdCardSize = sdBuffer;
            }

            ImGui::Spacing();

            ImGui::Text("GPU Mode");
            static const char *gpuModes[] = {"auto", "host", "swiftshader_indirect", "guest"};
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##gpu", gpuModes[m_SelectedGpuMode])) {
                for (int i = 0; i < 4; i++) {
                    const bool isSelected = (m_SelectedGpuMode == i);
                    if (ImGui::Selectable(gpuModes[i], isSelected)) {
                        m_SelectedGpuMode = i;
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

            const bool canCreate = !m_CreateParams.Name.empty() && !m_SystemImages.empty()
                                   && !m_DeviceProfiles.empty() && !formDisabled;

            if (isCreating) {
                ImGui::BeginDisabled();
                PositiveButton("Creating...", false, ImVec2(halfWidth, 0));
                ImGui::EndDisabled();
            } else {
                if (PositiveButton("Create", canCreate, ImVec2(halfWidth, 0))) {
                    m_CreateParams.SystemImagePackagePath = m_SystemImages[m_SelectedSystemImage].PackagePath;
                    m_CreateParams.DeviceId = m_DeviceProfiles[m_SelectedDevice].Id;
                    m_CreateParams.GpuMode = gpuModes[m_SelectedGpuMode];

                    m_AsyncBusy = true;
                    m_AsyncFuture = std::async(std::launch::async, [this]() {
                        CreateAvd(m_Sdk, m_CreateParams);
                        m_AsyncBusy = false;
                    });
                }
            }
            ImGui::SameLine();
            if (PrimaryButton("Cancel", !isCreating, ImVec2(halfWidth, 0))) {
                m_ShowCreateDialog = false;
            }

            // Check if async create finished
            if (!m_AsyncBusy && m_AsyncFuture.valid()) {
                m_AsyncFuture.get();
                m_ShowCreateDialog = false;
                m_RefreshAvds();
            }

            ImGui::Spacing();
        }
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

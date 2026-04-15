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

#include "imgui.h"
#include "imgui_internal.h"

#include "application.h"
#include "widgets.h"
#include "../core/onboarding.h"
#include "windows/about.h"
#include "windows/avd_info.h"
#include "windows/avd_list.h"
#include "windows/avd_logs.h"
#include "windows/avd_options.h"
#include "windows/create_avd.h"
#include "windows/delete_avd.h"
#include "windows/main_menu_bar.h"
#include "windows/onboarding.h"

namespace CoreDeck {
    Application::Application() : m_Context(DetectAndroidSdk()) {
        EnsureOptionsConfigDirectoryExists();

        if (!IsFirstRunComplete() || !m_Context.Sdk.IsFound) {
            m_Context.CurrentScreen = Screen::Onboarding;
        } else {
            m_Context.CurrentScreen = Screen::Main;
            RefreshAvds(m_Context);
        }
    }

    void Application::Build() {
        if (m_Context.CurrentScreen == Screen::Onboarding) {
            BuildOnboardingWindow(m_Context);
            return;
        }

        if (m_Context.SelectedAvd != m_Context.PreviousSelectedAvd) {
            if (m_Context.SelectedAvd >= 0 && m_Context.SelectedAvd < m_Context.Avds.size()) {
                const std::string &avdName = m_Context.Avds[m_Context.SelectedAvd].Name;
                if (!m_Context.PerAvdOptions.contains(avdName)) {
                    LoadAvdOptions(m_Context, avdName);
                }
            }
            m_Context.PreviousSelectedAvd = m_Context.SelectedAvd;
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

        BuildMainMenuBar(m_Context);
        BuildAvdOptionsWindow(m_Context);
        BuildAvdListWindow(m_Context);
        BuildAvdInfoWindow(m_Context);
        BuildAvdLogsWindow(m_Context);
        BuildAboutWindow(m_Context);
        BuildDeleteAvdWindow(m_Context);
        BuildCreateAvdWindow(m_Context);

        m_Context.Manager.Update();
    }

    void RefreshAvds(Context &context) {
        context.AvdNames = ListAvdNames(context.Sdk);
        context.Avds = LoadAvds(context.AvdNames);

        for (const auto &avdName: context.AvdNames) LoadAvdOptions(context, avdName);

        if (!context.Avds.empty()) context.SelectedAvd = 0;
        else context.SelectedAvd = -1;
        context.PreviousSelectedAvd = -1;
    }

    void LoadAvdOptions(Context &context, const std::string &avdName) {
        const std::string configPath = GetOptionsConfigPath(avdName);
        context.PerAvdOptions[avdName] = LoadOptionsFromFile(configPath);
    }

    void SaveAvdOptions(Context &context, const std::string &avdName) {
        if (!context.PerAvdOptions.contains(avdName)) return;

        const std::string configPath = GetOptionsConfigPath(avdName);
        SaveOptionsToFile(configPath, context.PerAvdOptions[avdName]);
    }

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context) {
        if (context.SelectedAvd >= 0 && context.SelectedAvd < context.Avds.size()) {
            const std::string &avdName = context.Avds[context.SelectedAvd].Name;

            if (!context.PerAvdOptions.contains(avdName)) LoadAvdOptions(context, avdName);
            return context.PerAvdOptions[avdName];
        }

        // Fallback
        static std::vector<EmulatorOption> defaultOptions = GetEmulatorOptions();
        return defaultOptions;
    }
}

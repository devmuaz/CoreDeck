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

#include <chrono>

#include "imgui.h"
#include "imgui_internal.h"

#include "application.h"
#include "../core/app_settings.h"
#include "../core/paths.h"
#include "windows/about.h"
#include "windows/avd_info.h"
#include "windows/avd_list.h"
#include "windows/avd_logs.h"
#include "windows/avd_options.h"
#include "windows/create_avd.h"
#include "windows/delete_avd.h"
#include "windows/install_image.h"
#include "windows/main_menu_bar.h"
#include "windows/onboarding.h"
#include "windows/preferences.h"
#include "windows/sdk_banner.h"
#include "windows/update.h"

#include "../core/version_check.h"

namespace CoreDeck {
    Application::Application() : m_Context(DetectAndroidSdk()) {
        EnsureOptionsConfigDirectoryExists();
        ApplyAppSettingsToContext(m_Context, LoadAppSettings());

        if (!Paths::Onboarding::IsFirstRunComplete() || !m_Context.Host.Sdk.IsFound) {
            m_Context.Flow.CurrentScreen = Screen::Onboarding;
        } else {
            m_Context.Flow.CurrentScreen = Screen::Main;
            RefreshAvds(m_Context);
        }
    }

    void Application::Build() {
        if (m_Context.Flow.CurrentScreen == Screen::Onboarding) {
            BuildOnboardingWindow(m_Context);
            return;
        }

#ifdef __APPLE__
        constexpr ImGuiKeyChord kPrimaryMod = ImGuiMod_Super;
#else
        constexpr ImGuiKeyChord kPrimaryMod = ImGuiMod_Ctrl;
#endif
        if (ImGui::Shortcut(kPrimaryMod | ImGuiKey_R) || ImGui::Shortcut(ImGuiKey_F5)) {
            RefreshAvds(m_Context);
        }
        if (ImGui::Shortcut(kPrimaryMod | ImGuiKey_Comma)) {
            m_Context.UI.ShowPreferences = true;
        }

        PollUpdateCheckIfNeeded();

        if (m_Context.Catalog.SelectedAvd != m_Context.Catalog.PreviousSelectedAvd) {
            if (m_Context.Catalog.SelectedAvd >= 0 && m_Context.Catalog.SelectedAvd < m_Context.Catalog.Avds.size()) {
                const std::string &avdName = m_Context.Catalog.Avds[m_Context.Catalog.SelectedAvd].Name;
                if (!m_Context.Catalog.PerAvdOptions.contains(avdName)) {
                    LoadAvdOptions(m_Context, avdName);
                }
            }
            m_Context.Catalog.PreviousSelectedAvd = m_Context.Catalog.SelectedAvd;
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
        BuildSdkMissingBanner(m_Context);
        BuildDeleteAvdWindow(m_Context);
        BuildAvdOptionsWindow(m_Context);
        BuildAvdListWindow(m_Context);
        BuildAvdInfoWindow(m_Context);
        BuildAvdLogsWindow(m_Context);
        BuildAboutWindow(m_Context);
        BuildPreferencesWindow(m_Context);
        BuildUpdateNoticeWindow(m_Context);
        BuildCreateAvdWindow(m_Context);
        BuildInstallImageWindow(m_Context);

        m_Context.Host.Manager.Update();
    }

    void Application::PollUpdateCheckIfNeeded() {
        if (m_UpdateCheckFuture.valid()) {
            if (m_UpdateCheckFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }

            std::optional<std::string> newer = m_UpdateCheckFuture.get();
            m_Context.Updates.UpdateCheckInFlight = false;

            if (newer) {
                m_Context.Updates.LatestVersion = std::move(*newer);
                m_Context.Updates.ShowNewVersionModal = true;
            } else if (m_UpdateCheckWasManual) {
                m_Context.Updates.ShowUpToDateModal = true;
            }
            m_UpdateCheckWasManual = false;
            return;
        }

        bool start = false;
        if (!m_AutoUpdateCheckStarted) {
            m_AutoUpdateCheckStarted = true;
            m_UpdateCheckWasManual = false;
            start = true;
        } else if (m_Context.Updates.RequestManualUpdateCheck) {
            m_Context.Updates.RequestManualUpdateCheck = false;
            m_UpdateCheckWasManual = true;
            start = true;
        }

        if (start) {
            m_Context.Updates.UpdateCheckInFlight = true;
            m_UpdateCheckFuture = std::async(std::launch::async, []() -> std::optional<std::string> {
                try {
                    return QueryRemoteNewerVersion();
                } catch (...) {
                    return std::nullopt;
                }
            });
        }
    }

    void Application::SetMainWindow(GLFWwindow *const window) {
        m_Context.UI.MainWindow = window;
    }

    AppSettings CaptureAppSettingsFromContext(const Context &context) {
        AppSettings s;
        s.SchemaVersion = 1;
        s.AutoScroll = context.Logs.AutoScroll;
        s.ConfirmBeforeDeleteAvd = context.Prefs.ConfirmBeforeDeleteAvd;
        s.ShowAvdListPanel = context.UI.ShowAvdListPanel;
        s.ShowOptionsPanel = context.UI.ShowOptionsPanel;
        s.ShowDetailsPanel = context.UI.ShowDetailsPanel;
        s.ShowLogPanel = context.UI.ShowLogPanel;
        s.AvdSortMode = static_cast<int>(context.Catalog.SortMode);
        s.AvdSortAscending = context.Catalog.SortAscending;
        return s;
    }

    void ApplyAppSettingsToContext(Context &context, const AppSettings &settings) {
        context.Logs.AutoScroll = settings.AutoScroll;
        context.Prefs.ConfirmBeforeDeleteAvd = settings.ConfirmBeforeDeleteAvd;
        context.UI.ShowAvdListPanel = settings.ShowAvdListPanel;
        context.UI.ShowOptionsPanel = settings.ShowOptionsPanel;
        context.UI.ShowDetailsPanel = settings.ShowDetailsPanel;
        context.UI.ShowLogPanel = settings.ShowLogPanel;

        if (const int sortMode = settings.AvdSortMode; sortMode >= 0 && sortMode <= 2) {
            context.Catalog.SortMode = static_cast<AvdSortMode>(sortMode);
        }
        context.Catalog.SortAscending = settings.AvdSortAscending;
    }

    void PersistAppSettings(const Context &context) {
        SaveAppSettings(CaptureAppSettingsFromContext(context));
    }

    void RefreshAvds(Context &context) {
        context.Catalog.AvdNames = ListAvdNames(context.Host.Sdk);
        context.Catalog.Avds = LoadAvds(context.Catalog.AvdNames);

        for (const auto &avdName: context.Catalog.AvdNames) LoadAvdOptions(context, avdName);

        if (!context.Catalog.Avds.empty()) context.Catalog.SelectedAvd = 0;
        else context.Catalog.SelectedAvd = -1;
        context.Catalog.PreviousSelectedAvd = -1;
    }

    void LoadAvdOptions(Context &context, const std::string &avdName) {
        const std::string configPath = GetOptionsConfigPath(avdName);
        context.Catalog.PerAvdOptions[avdName] = LoadOptionsFromFile(configPath);
    }

    void SaveAvdOptions(Context &context, const std::string &avdName) {
        if (!context.Catalog.PerAvdOptions.contains(avdName)) return;

        const std::string configPath = GetOptionsConfigPath(avdName);
        SaveOptionsToFile(configPath, context.Catalog.PerAvdOptions[avdName]);
    }

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context) {
        if (context.Catalog.SelectedAvd >= 0 && context.Catalog.SelectedAvd < context.Catalog.Avds.size()) {
            const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;

            if (!context.Catalog.PerAvdOptions.contains(avdName)) LoadAvdOptions(context, avdName);
            return context.Catalog.PerAvdOptions[avdName];
        }

        // Fallback
        static std::vector<EmulatorOption> defaultOptions = GetEmulatorOptions();
        return defaultOptions;
    }
}

//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <GLFW/glfw3native.h>
#endif

#include "application.h"
#include "theme.h"
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
#include "windows/storage.h"
#include "windows/update.h"
#include "../core/version_check.h"

namespace CoreDeck {
    namespace {
        void ShowFatalError(const char *title, const char *message) {
#if defined(_WIN32)
            MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
#else
            (void) title;
            (void) std::fprintf(stderr, "%s\n", message);
#endif
        }
    }

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


    int Application::Run() {
        if (!m_InitPlatform()) {
            return 1;
        }
        if (!m_CreateMainWindow()) {
            return 1;
        }

        m_InitImGui();
        m_ApplyDpiScale();
        m_LoadFonts();

        ImGui::StyleColorsDark();
        ApplyCustomImGuiTheme(m_DpiScale);

        const char *glslVersion = "#version 330";
        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init(glslVersion);
        m_ImGuiBackendsInitialized = true;
        m_SetupCallbacks();
        m_RunLoop();
        m_Shutdown();

        return 0;
    }

    Application::~Application() {
        m_Shutdown();
    }

    void Application::m_Build() {
        if (m_Context.Flow.CurrentScreen == Screen::Onboarding) {
            BuildOnboardingWindow(m_Context);
            return;
        }

#ifdef NDEBUG
        m_PollUpdateCheckIfNeeded();
#endif

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

                ImGuiID topId = 0;
                ImGuiID bottomId = 0;
                ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Down, 0.40F, &bottomId, &topId);

                ImGuiID leftId = 0;
                ImGuiID centerId = 0;
                ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.25, &leftId, &centerId);

                ImGuiID middleId = 0;
                ImGuiID rightId = 0;
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.35F, &rightId, &middleId);

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
        if (!m_Context.UI.ShowCreateAvdDialog) {
            BuildInstallImageWindow(m_Context);
        }
        BuildStorageWindow(m_Context);

        m_Context.Host.Manager.Update();
    }


    bool Application::m_InitPlatform() {
        if (!glfwInit()) {
            ShowFatalError(COREDECK_TITLE, "Failed to initialize GLFW.");
            return false;
        }
        m_GlfwInitialized = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef GLFW_SCALE_FRAMEBUFFER
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#endif
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
        return true;
    }

    bool Application::m_CreateMainWindow() {
        m_Window = glfwCreateWindow(1200, 900, COREDECK_TITLE, nullptr, nullptr);
        if (!m_Window) {
            ShowFatalError(COREDECK_TITLE, "Failed to create window.\nYour system may not support OpenGL 3.3.");
            return false;
        }

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

#if defined(_WIN32)
        HWND hwnd = glfwGetWin32Window(m_Window);
        HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
#endif

        m_Context.UI.MainWindow = m_Window;
        return true;
    }

    void Application::m_InitImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_ImGuiContextCreated = true;

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // Do NOT enable ConfigDpiScaleFonts. It instructs ImGui to overwrite
        // style.FontScaleDpi every time DisplayFramebufferScale changes, which
        // would compound with the per-platform scaling we apply explicitly in
        // m_ApplyDpiScale() + m_LoadFonts() and produce text that's roughly
        // square-of-scale too large (e.g. 2.25x at Windows 150%), causing text
        // to overflow buttons and sub-windows. We own font sizing ourselves.
        //
        // ConfigDpiScaleViewports stays on -- it only affects ImGui's
        // per-viewport rectangle math for multi-monitor docking, not fonts.
        io.ConfigDpiScaleViewports = true;

        static std::string imguiIniPath = Paths::GetAppConfigPath("imgui.ini");
        io.IniFilename = imguiIniPath.c_str();
    }

    void Application::m_LoadFonts() const {
        const ImGuiIO &io = ImGui::GetIO();

        const std::string resourcesDir = Paths::GetResourcesDirectory();
        const std::string textFontPath = Paths::JoinPaths(
            {resourcesDir, "assets", "fonts", "JetBrainsMono-Regular.ttf"}
        );
        const std::string iconFontPath = Paths::JoinPaths(
            {resourcesDir, "assets", "fonts", "FontAwesome7Free-Solid-900.otf"}
        );

        const float dpi = (m_FontPixelScale > 0.0F) ? m_FontPixelScale : 1.0F;
        constexpr float BASE_TEXT_SIZE = 16.0F;
        constexpr float BASE_ICON_SIZE = 12.0F;
        constexpr float BASE_GLYPH_MIN_ADVANCE = 16.0F;

        if (std::filesystem::exists(textFontPath)) {
            static constexpr ImWchar TEXT_RANGES[] = {
                0x0020,
                0x00FF,
                0x2000,
                0x206F,
                0,
            };
            io.Fonts->AddFontFromFileTTF(textFontPath.c_str(), BASE_TEXT_SIZE * dpi, nullptr, TEXT_RANGES);
        }

        if (std::filesystem::exists(iconFontPath)) {
            ImFontConfig iconConfig;
            iconConfig.MergeMode = true;
            iconConfig.PixelSnapH = true;
            iconConfig.GlyphMinAdvanceX = BASE_GLYPH_MIN_ADVANCE * dpi;

            static constexpr ImWchar ICON_RANGES[] = {0xf000, 0xf8ff, 0};
            io.Fonts->AddFontFromFileTTF(iconFontPath.c_str(), BASE_ICON_SIZE * dpi, &iconConfig, ICON_RANGES);
        }
    }

    void Application::m_ApplyDpiScale() {
        float xscale = 1.0F;
        float yscale = 1.0F;
        if (m_Window) {
            glfwGetWindowContentScale(m_Window, &xscale, &yscale);
        }
        const float reportedScale = (xscale > 0.0F) ? xscale : 1.0F;

#if defined(__APPLE__)
        m_DpiScale = 1.0F;
        m_FontPixelScale = 1.0F;
#else
        m_DpiScale = reportedScale;
        m_FontPixelScale = reportedScale;
#endif
        ImGui::GetStyle().FontScaleDpi = 1.0F;
    }

    void Application::m_SetupCallbacks() {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetScrollCallback(m_Window, [](GLFWwindow *, const double x, const double y) {
            ImGuiIO &imGuiIO = ImGui::GetIO();
            imGuiIO.AddMouseWheelEvent(static_cast<float>(x) * 0.3F, static_cast<float>(y) * 0.3F);
        });

#if !defined(__APPLE__)
        glfwSetWindowContentScaleCallback(m_Window, [](GLFWwindow *w, const float xscale, const float /*yscale*/) {
            auto *self = static_cast<Application *>(glfwGetWindowUserPointer(w));
            if (self == nullptr || xscale <= 0.0F) {
                return;
            }
            if (std::abs(xscale - self->m_DpiScale) < 0.01F) {
                return;
            }

            self->m_DpiScale = xscale;
            self->m_FontPixelScale = xscale;
            ImGui::GetStyle().FontScaleDpi = 1.0F;

            ImGuiIO &io = ImGui::GetIO();
            io.Fonts->Clear();
            self->m_LoadFonts();
            io.Fonts->Build();

            ImGui::StyleColorsDark();
            ApplyCustomImGuiTheme(self->m_DpiScale);
        });
#endif

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow *w, const int width, const int height) {
            if (width == 0 || height == 0) {
                return;
            }

            auto *self = static_cast<Application *>(glfwGetWindowUserPointer(w));

            glViewport(0, 0, width, height);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            self->m_Build();

            ImGui::Render();
            glClearColor(0.06F, 0.06F, 0.07F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(w);
        });
    }

    void Application::m_RunLoop() {
        while (!glfwWindowShouldClose(m_Window)) {
            const bool focused = glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) != 0;
            const bool hovered = glfwGetWindowAttrib(m_Window, GLFW_HOVERED) != 0;
            const double timeout = focused && hovered ? 1.0 / 60.0 : 0.25;
            glfwWaitEventsTimeout(timeout);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            m_Build();

            ImGui::Render();
            int displayW = 0;
            int displayH = 0;
            glfwGetFramebufferSize(m_Window, &displayW, &displayH);
            glViewport(0, 0, displayW, displayH);
            glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(m_Window);
        }
    }

    void Application::m_Shutdown() {
        if (m_ImGuiBackendsInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            m_ImGuiBackendsInitialized = false;
        }
        if (m_ImGuiContextCreated) {
            ImGui::DestroyContext();
            m_ImGuiContextCreated = false;
        }
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        if (m_GlfwInitialized) {
            glfwTerminate();
            m_GlfwInitialized = false;
        }
    }

    void Application::m_PollUpdateCheckIfNeeded() {
        if (m_UpdateCheckFuture.valid()) {
            if (m_UpdateCheckFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }

            std::optional<RemoteRelease> newer = m_UpdateCheckFuture.get();
            m_Context.Updates.UpdateCheckInFlight = false;

            if (newer) {
                m_Context.Updates.LatestVersion = std::move(newer->Version);
                m_Context.Updates.LatestNotes = std::move(newer->Notes);
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
            m_UpdateCheckFuture = std::async(std::launch::async, []() -> std::optional<RemoteRelease> {
                try {
                    return QueryRemoteNewerVersion();
                } catch (...) {
                    return std::nullopt;
                }
            });
        }
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

        for (const auto &avdName: context.Catalog.AvdNames) {
            LoadAvdOptions(context, avdName);
        }

        context.DiskUsage.PerAvdCache.clear();
        if (!context.DiskUsage.Loading.load()) {
            context.DiskUsage.LastScan = {};
            context.DiskUsage.Ready = false;
        }

        if (!context.Catalog.Avds.empty()) {
            context.Catalog.SelectedAvd = 0;
        } else {
            context.Catalog.SelectedAvd = -1;
        }
        context.Catalog.PreviousSelectedAvd = -1;
    }

    void LoadAvdOptions(Context &context, const std::string &avdName) {
        const std::string configPath = GetOptionsConfigPath(avdName);
        context.Catalog.PerAvdOptions[avdName] = LoadOptionsFromFile(configPath);
    }

    void SaveAvdOptions(Context &context, const std::string &avdName) {
        if (!context.Catalog.PerAvdOptions.contains(avdName)) {
            return;
        }

        const std::string configPath = GetOptionsConfigPath(avdName);
        SaveOptionsToFile(configPath, context.Catalog.PerAvdOptions[avdName]);
    }

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context) {
        if (context.Catalog.SelectedAvd >= 0 && context.Catalog.SelectedAvd < context.Catalog.Avds.size()) {
            const std::string &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;

            if (!context.Catalog.PerAvdOptions.contains(avdName)) {
                LoadAvdOptions(context, avdName);
            }
            return context.Catalog.PerAvdOptions[avdName];
        }

        // Fallback
        static std::vector<EmulatorOption> defaultOptions = GetEmulatorOptions();
        return defaultOptions;
    }
}

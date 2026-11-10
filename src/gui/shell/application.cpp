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
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <dwmapi.h>
#endif

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <atomic>
#include <chrono>
#ifndef __APPLE__
#include <cmath>
#endif
#include <cstdio>
#include <filesystem>
#include <utility>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#if defined(_WIN32) || (defined(__APPLE__) && defined(__clang__))
#include <GLFW/glfw3native.h>
#elif defined(__APPLE__)
// glfw3native.h pulls in Apple headers that use blocks, which GCC cannot parse.
#include <objc/objc.h>
extern "C" id glfwGetCocoaWindow(GLFWwindow *window);
#endif

#include "gui/shell/application.h"
#include "app/app_settings.h"
#include "gui/theme/theme.h"
#include "gui/widgets/widgets.h"
#include "platform/build_info.h"
#include "platform/i18n.h"
#include "platform/paths.h"
#ifdef __linux__
#include "platform/log.h"
#include "platform/png.h"
#endif
#include "app/app_paths.h"
#include "app/version_check.h"
#include "gui/adb/adb_workspace.h"
#include "gui/apk/apk_analyzer_workspace.h"
#include "gui/apk/apk_signer_workspace.h"
#include "gui/apk/jar_signer_workspace.h"
#include "gui/apk/keytool_workspace.h"
#include "gui/home/avd_details_panel.h"
#include "gui/home/avd_list_panel.h"
#include "gui/home/avd_logs_panel.h"
#include "gui/home/avd_options_panel.h"
#include "gui/home/create_avd_dialog.h"
#include "gui/home/install_image_dialog.h"
#include "gui/home/storage_dialog.h"
#include "gui/preferences/preferences_dialog.h"
#include "gui/sdk/health_check_banner.h"
#include "gui/sdk/health_check_dialog.h"
#include "gui/sdk/onboarding_screen.h"
#include "gui/shell/about_dialog.h"
#include "gui/shell/main_menu_bar.h"
#include "gui/shell/update_dialog.h"

namespace CoreDeck::Gui {
    namespace {
        void ClearFramebuffer() {
            const ImVec4 color = HexColor(Colors::SURFACE0);
            glClearColor(color.x, color.y, color.z, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        void ShowFatalError(const char *title, const char *message) {
#ifdef _WIN32
            MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
#else
            (void) title;
            (void) std::fprintf(stderr, "%s\n", message);
#endif
        }
    }

    Application::Application() : m_Context(Core::DetectAndroidSdk(App::LoadAppSettings().SdkPath)) {
        App::EnsureOptionsConfigDirectoryExists();
        ApplyAppSettingsToContext(m_Context, App::LoadAppSettings());
        Platform::SetLocalesDirectory(Platform::JoinPaths({App::GetAssetsDirectory(), "locales"}));
        Platform::SetLanguage(m_Context.Prefs.Language);

        m_Context.Host.Jdk = Core::DetectJdk(App::LoadAppSettings().JdkPath);
        Core::ApplyJdkToSdk(m_Context.Host.Sdk, m_Context.Host.Jdk);
        m_Context.Host.SyncSdk();

        if (!App::LoadAppSettings().FirstRunComplete || !m_Context.Host.Sdk.IsFound) {
            m_Context.Shell.CurrentScreen = Screen::Onboarding;
        } else {
            m_Context.Shell.CurrentScreen = Screen::Main;
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

        ApplyCustomImGuiTheme(m_DpiScale);
        ApplyWindowChrome(m_Window);

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

    void Application::m_ApplyNativeWindowTitle() {
        if (m_AppliedWindowPage == m_Context.Shell.NativeWindowPage) {
            return;
        }
        m_AppliedWindowPage = m_Context.Shell.NativeWindowPage;
        const std::string title = m_AppliedWindowPage.empty()
                                      ? std::string(Platform::BuildInfo::TITLE)
                                      : std::string(Platform::BuildInfo::TITLE) + " - " + m_AppliedWindowPage;
        glfwSetWindowTitle(m_Window, title.c_str());
    }

    void Application::m_Build() {
        PollAvdListRefresh(m_Context);
        m_Context.Shell.NativeWindowPage.clear();

        if (m_Context.Shell.CurrentScreen == Screen::Onboarding) {
            BuildOnboardingScreen(m_Context);
            m_ApplyNativeWindowTitle();
            return;
        }

        if (m_Context.Host.Sdk.SdkPath.empty()) {
            const auto toolEnv = m_Context.Host.Sdk.ToolEnv;
            m_Context.Host.Sdk = Core::DetectAndroidSdk(App::LoadAppSettings().SdkPath);
            m_Context.Host.Sdk.ToolEnv = toolEnv;
            m_Context.Host.SyncSdk();
        } else {
            const std::string previousAdbPath = m_Context.Host.Sdk.AdbPath;
            Core::RefreshAndroidSdk(m_Context.Host.Sdk);
            if (m_Context.Host.Sdk.AdbPath != previousAdbPath) {
                m_Context.Host.SyncSdk();
            }
        }

#ifdef NDEBUG
        m_PollUpdateCheckIfNeeded();
#endif

        if (m_Context.Workspaces.Home.Catalog.SelectedAvd != m_Context.Workspaces.Home.Catalog.PreviousSelectedAvd) {
            if (m_Context.Workspaces.Home.Catalog.SelectedAvd >= 0 && m_Context.Workspaces.Home.Catalog.SelectedAvd < m_Context.Workspaces.Home.Catalog.Avds.size()) {
                const auto &avd = m_Context.Workspaces.Home.Catalog.Avds.at(m_Context.Workspaces.Home.Catalog.SelectedAvd);
                const std::string &avdName = avd.Name;
                if (!m_Context.Workspaces.Home.Catalog.PerAvdOptions.contains(avdName)) {
                    LoadAvdOptions(m_Context, avdName);
                }
            }
            m_Context.Workspaces.Home.Catalog.PreviousSelectedAvd = m_Context.Workspaces.Home.Catalog.SelectedAvd;
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

                const auto configureNode = [](const ImGuiID id) {
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
        if (m_Context.Shell.Workspace == WorkspacePage::Home) {
            BuildHealthCheckBanner(m_Context);
            BuildAvdOptionsPanel(m_Context);
            BuildAvdListPanel(m_Context);
            BuildAvdDetailsPanel(m_Context);
            BuildAvdLogsPanel(m_Context);
        }
        BuildDeleteAvdDialog(m_Context);
        BuildAboutDialog(m_Context);
        BuildPreferencesDialog(m_Context);
        BuildUpdateDialog(m_Context);
        BuildCreateAvdDialog(m_Context);
        if (!m_Context.Dialogs.CreateAvd.Open) {
            BuildInstallImageDialog(m_Context);
        }
        BuildHealthCheckDialog(m_Context);
        BuildAdbWorkspace(m_Context);
        BuildApkAnalyzerWorkspace(m_Context);
        BuildKeytoolWorkspace(m_Context);
        BuildApkSignerWorkspace(m_Context);
        BuildJarSignerWorkspace(m_Context);
        BuildStorageDialog(m_Context);

        m_Context.Host.Manager.Update();
        m_ApplyNativeWindowTitle();
    }


    bool Application::m_InitPlatform() {
        if (!glfwInit()) {
            ShowFatalError(Platform::BuildInfo::TITLE, Platform::Tr("Failed to initialize GLFW."));
            return false;
        }
        m_GlfwInitialized = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

#ifdef __linux__
        glfwWindowHintString(GLFW_X11_CLASS_NAME, Platform::BuildInfo::APP_ID);
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, Platform::BuildInfo::APP_ID);
#endif

        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef GLFW_SCALE_FRAMEBUFFER
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#endif
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
        return true;
    }

    bool Application::m_CreateMainWindow() {
        m_Window = glfwCreateWindow(1200, 900, Platform::BuildInfo::TITLE, nullptr, nullptr);
        if (!m_Window) {
            ShowFatalError(Platform::BuildInfo::TITLE, Platform::Tr("Failed to create window.\nYour system may not support OpenGL 3.3."));
            return false;
        }

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

#ifdef __linux__
        const std::string iconPath = Platform::JoinPaths({App::GetAssetsDirectory(), "icons", "icon.png"});
        if (auto icon = Platform::DecodePngFile(iconPath)) {
            GLFWimage image{};
            image.width = icon->Width;
            image.height = icon->Height;
            image.pixels = icon->Rgba.data();
            glfwSetWindowIcon(m_Window, 1, &image);
        } else {
            Platform::Log::Warn("Window icon was not loaded: ", iconPath);
        }
#endif

#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(m_Window);
        HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
#endif

        m_Context.Shell.MainWindow = m_Window;
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

        static const std::string IMGUI_INI_PATH = App::GetAppConfigPath("imgui.ini");
        io.IniFilename = IMGUI_INI_PATH.c_str();
    }

    void Application::m_LoadFonts() const {
        const ImGuiIO &io = ImGui::GetIO();

        const std::string assetsDir = App::GetAssetsDirectory();
        const std::string textFontPath = Platform::JoinPaths(
            {assetsDir, "fonts", "JetBrainsMono-Regular.ttf"}
        );
        const std::string cjkFontPath = Platform::JoinPaths(
            {assetsDir, "fonts", "NotoSansCJKsc-Regular.otf"}
        );
        const std::string iconFontPath = Platform::JoinPaths(
            {assetsDir, "fonts", "FontAwesome7Free-Solid-900.otf"}
        );

        const float dpi = (m_FontPixelScale > 0.0F) ? m_FontPixelScale : 1.0F;
        constexpr float BASE_TEXT_SIZE = 16.0F;
        constexpr float BASE_ICON_SIZE = 12.0F;
        constexpr float BASE_GLYPH_MIN_ADVANCE = 16.0F;
        static constexpr ImWchar ICON_RANGES[] = {0xf000, 0xf8ff, 0};

        if (std::filesystem::exists(textFontPath)) {
            io.Fonts->AddFontFromFileTTF(textFontPath.c_str(), BASE_TEXT_SIZE * dpi, nullptr, nullptr);
        }

        if (std::filesystem::exists(cjkFontPath)) {
            ImFontConfig cjkConfig;
            cjkConfig.MergeMode = true;
            cjkConfig.GlyphExcludeRanges = ICON_RANGES;
            io.Fonts->AddFontFromFileTTF(cjkFontPath.c_str(), BASE_TEXT_SIZE * dpi, &cjkConfig, nullptr);
        }

        if (std::filesystem::exists(iconFontPath)) {
            ImFontConfig iconConfig;
            iconConfig.MergeMode = true;
            iconConfig.PixelSnapH = true;
            iconConfig.GlyphMinAdvanceX = BASE_GLYPH_MIN_ADVANCE * dpi;

            io.Fonts->AddFontFromFileTTF(iconFontPath.c_str(), BASE_ICON_SIZE * dpi, &iconConfig, ICON_RANGES);
        }
    }

    void Application::m_ApplyDpiScale() {
#ifdef __APPLE__
        m_DpiScale = 1.0F;
        m_FontPixelScale = 1.0F;
#else
        float xscale = 1.0F;
        float yscale = 1.0F;
        if (m_Window) {
            glfwGetWindowContentScale(m_Window, &xscale, &yscale);
        }
        const float reportedScale = (xscale > 0.0F) ? xscale : 1.0F;
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

#ifndef __APPLE__
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

            const ImGuiIO &io = ImGui::GetIO();
            io.Fonts->Clear();
            self->m_LoadFonts();
            io.Fonts->Build();

            ApplyCustomImGuiTheme(self->m_DpiScale);
        });
#endif

        glfwSetDropCallback(m_Window, [](GLFWwindow *w, const int count, const char **paths) {
            auto *self = static_cast<Application *>(glfwGetWindowUserPointer(w));
            if (self == nullptr || paths == nullptr) {
                return;
            }
            auto &pending = self->m_Context.Workspaces.ApkAnalyzer.PendingDrops;
            for (int i = 0; i < count; ++i) {
                if (paths[i] == nullptr) {
                    continue;
                }
                const std::string path(paths[i]);
                const auto dot = path.rfind('.');
                if (dot == std::string::npos) {
                    continue;
                }
                const std::string ext = path.substr(dot);
                if (ext != ".apk" && ext != ".APK" && ext != ".Apk") {
                    continue;
                }
                pending.push_back(path);
            }
            if (!pending.empty()) {
                self->m_Context.Shell.Workspace = WorkspacePage::ApkAnalyzer;
            }
        });

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
            ClearFramebuffer();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(w);
        });
    }

    void Application::m_RunLoop() {
        while (!glfwWindowShouldClose(m_Window)) {
            const bool focused = glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) != 0;
            const bool hovered = glfwGetWindowAttrib(m_Window, GLFW_HOVERED) != 0;
            const bool animate = ConsumeSpinnerFrameRequest();
            const double timeout = focused && (hovered || animate) ? 1.0 / 60.0 : 0.25;
            glfwWaitEventsTimeout(timeout);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (SyncSystemColorScheme()) {
                RefreshThemeColors();
            }
            ApplyWindowChrome(m_Window);

            m_Build();

            ImGui::Render();
            int displayW = 0;
            int displayH = 0;
            glfwGetFramebufferSize(m_Window, &displayW, &displayH);
            glViewport(0, 0, displayW, displayH);
            ClearFramebuffer();
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

            std::optional<App::RemoteRelease> newer = m_UpdateCheckFuture.get();
            m_Context.Dialogs.Update.UpdateCheckInFlight = false;

            if (newer) {
                m_Context.Dialogs.Update.LatestVersion = std::move(newer->Version);
                m_Context.Dialogs.Update.LatestNotes = std::move(newer->Notes);
                OpenUpdateDialog(m_Context);
            } else if (m_UpdateCheckWasManual) {
                OpenUpToDateDialog(m_Context);
            }
            m_UpdateCheckWasManual = false;
            return;
        }

        bool start = false;
        if (!m_AutoUpdateCheckStarted) {
            m_AutoUpdateCheckStarted = true;
            m_UpdateCheckWasManual = false;
            start = m_Context.Prefs.CheckForUpdatesOnLaunch;
        } else if (m_Context.Dialogs.Update.RequestManualUpdateCheck) {
            m_Context.Dialogs.Update.RequestManualUpdateCheck = false;
            m_UpdateCheckWasManual = true;
            start = true;
        }

        if (start) {
            m_Context.Dialogs.Update.UpdateCheckInFlight = true;
            const bool includeBetaReleases = m_Context.Prefs.IncludeBetaReleases;
            m_UpdateCheckFuture = std::async(std::launch::async, [includeBetaReleases]() -> std::optional<App::RemoteRelease> {
                try {
                    return App::QueryRemoteNewerVersion(includeBetaReleases);
                } catch (...) {
                    return std::nullopt;
                }
            });
        }
    }

    App::AppSettings CaptureAppSettingsFromContext(const Context &context) {
        App::AppSettings s = App::LoadAppSettings();
        s.SchemaVersion = 1;
        s.AutoScroll = context.Prefs.AutoScroll;
        s.ConfirmBeforeDeleteAvd = context.Prefs.ConfirmBeforeDeleteAvd;
        s.CrashReportingEnabled = context.Prefs.CrashReportingEnabled;
        s.CheckForUpdatesOnLaunch = context.Prefs.CheckForUpdatesOnLaunch;
        s.IncludeBetaReleases = context.Prefs.IncludeBetaReleases;
        s.ShowAvdListPanel = context.Shell.AvdListPanel;
        s.ShowOptionsPanel = context.Shell.OptionsPanel;
        s.ShowDetailsPanel = context.Shell.DetailsPanel;
        s.ShowLogPanel = context.Shell.LogPanel;
        s.AvdSortMode = static_cast<int>(context.Workspaces.Home.Catalog.SortMode);
        s.AvdSortAscending = context.Workspaces.Home.Catalog.SortAscending;
        s.Theme = static_cast<int>(context.Prefs.Theme);
        s.Language = context.Prefs.Language;
        return s;
    }

    void ApplyAppSettingsToContext(Context &context, const App::AppSettings &settings) {
        context.Prefs.AutoScroll = settings.AutoScroll;
        context.Prefs.ConfirmBeforeDeleteAvd = settings.ConfirmBeforeDeleteAvd;
        context.Prefs.CrashReportingEnabled = settings.CrashReportingEnabled;
        context.Prefs.CheckForUpdatesOnLaunch = settings.CheckForUpdatesOnLaunch;
        context.Prefs.IncludeBetaReleases = settings.IncludeBetaReleases;
        context.Shell.AvdListPanel = settings.ShowAvdListPanel;
        context.Shell.OptionsPanel = settings.ShowOptionsPanel;
        context.Shell.DetailsPanel = settings.ShowDetailsPanel;
        context.Shell.LogPanel = settings.ShowLogPanel;

        if (const int sortMode = settings.AvdSortMode; sortMode >= 0 && sortMode <= 2) {
            context.Workspaces.Home.Catalog.SortMode = static_cast<AvdSortMode>(sortMode);
        }
        context.Workspaces.Home.Catalog.SortAscending = settings.AvdSortAscending;

        const int theme = settings.Theme;
        const auto preference = theme >= static_cast<int>(App::ThemePreference::System) &&
                                        theme <= static_cast<int>(App::ThemePreference::Light)
                                    ? static_cast<App::ThemePreference>(theme)
                                    : App::ThemePreference::System;
        context.Prefs.Theme = preference;
        context.Prefs.Language = settings.Language;
        SetThemePreference(preference);
    }

    void ApplyWindowChrome(GLFWwindow *window) { // NOLINT(misc-const-correctness) Windows and macOS pass this pointer to GLFW.
        if (window == nullptr) {
            return;
        }

        const bool light = IsLightColorScheme();
        static const GLFWwindow *appliedWindow = nullptr;
        static bool appliedLight = false;
        static bool applied = false;
        if (applied && appliedWindow == window && appliedLight == light) {
            return;
        }
        applied = true;
        appliedWindow = window;
        appliedLight = light;

#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(window);
        if (hwnd != nullptr) {
            const BOOL useDark = light ? FALSE : TRUE;
            if (FAILED(DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark)))) {
                DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
            }
        }
#elif defined(__APPLE__)
        SetCocoaWindowAppearance(glfwGetCocoaWindow(window), light);
#endif
    }

    void PersistAppSettings(const Context &context) {
        App::SaveAppSettings(CaptureAppSettingsFromContext(context));
    }

    namespace {
        struct AvdListRefreshJob {
            std::future<std::pair<int, AvdCatalogSnapshot>> Future;
            bool Queued = false;
        };

        AvdListRefreshJob &ListRefreshJob() {
            static AvdListRefreshJob job;
            return job;
        }

        std::atomic<int> g_AvdCatalogEpoch{0};
    }

    AvdCatalogSnapshot LoadAvdCatalog(const Core::SdkInfo &sdk) {
        AvdCatalogSnapshot snapshot;
        snapshot.Names = Core::ListAvdNames(sdk);
        snapshot.Avds = Core::LoadAvds(snapshot.Names);
        snapshot.Options.reserve(snapshot.Names.size());
        for (const std::string &avdName: snapshot.Names) {
            snapshot.Options.emplace(avdName, Core::LoadOptionsFromFile(App::GetOptionsConfigPath(avdName)));
        }
        return snapshot;
    }

    void ApplyAvdCatalog(Context &context, AvdCatalogSnapshot snapshot) {
        context.Workspaces.Home.Catalog.AvdNames = std::move(snapshot.Names);
        context.Workspaces.Home.Catalog.Avds = std::move(snapshot.Avds);
        for (const std::string &avdName: context.Workspaces.Home.Catalog.AvdNames) {
            const auto found = snapshot.Options.find(avdName);
            if (found != snapshot.Options.end()) {
                context.Workspaces.Home.Catalog.PerAvdOptions[avdName] = std::move(found->second);
            }
        }

        context.Workspaces.Home.DiskUsage.PerAvdCache.clear();
        if (!context.Workspaces.Home.DiskUsage.Loading.load()) {
            context.Workspaces.Home.DiskUsage.LastScan = {};
            context.Workspaces.Home.DiskUsage.Ready = false;
        }

        if (!context.Workspaces.Home.Catalog.Avds.empty()) {
            context.Workspaces.Home.Catalog.SelectedAvd = 0;
        } else {
            context.Workspaces.Home.Catalog.SelectedAvd = -1;
        }
        context.Workspaces.Home.Catalog.PreviousSelectedAvd = -1;
    }

    void RefreshAvds(Context &context) {
        ApplyAvdCatalog(context, LoadAvdCatalog(context.Host.Sdk));
    }

    void RequestAvdListRefresh(const Context &context) {
        AvdListRefreshJob &job = ListRefreshJob();
        if (job.Future.valid()) {
            job.Queued = true;
            return;
        }

        const int epoch = ++g_AvdCatalogEpoch;
        const Core::SdkInfo sdk = context.Host.Sdk;
        job.Future = std::async(std::launch::async, [sdk, epoch] {
            return std::make_pair(epoch, LoadAvdCatalog(sdk));
        });
    }

    void PollAvdListRefresh(Context &context) {
        AvdListRefreshJob &job = ListRefreshJob();
        if (!job.Future.valid()) {
            return;
        }
        if (job.Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }

        auto [epoch, snapshot] = job.Future.get();
        if (epoch == g_AvdCatalogEpoch.load()) {
            ApplyAvdCatalog(context, std::move(snapshot));
        }
        if (job.Queued) {
            job.Queued = false;
            RequestAvdListRefresh(context);
        }
    }

    void SupersedeAvdListRefresh() {
        ++g_AvdCatalogEpoch;
        ListRefreshJob().Queued = false;
    }

    void LoadAvdOptions(Context &context, const std::string &avdName) {
        const std::string configPath = App::GetOptionsConfigPath(avdName);
        context.Workspaces.Home.Catalog.PerAvdOptions[avdName] = Core::LoadOptionsFromFile(configPath);
    }

    void SaveAvdOptions(Context &context, const std::string &avdName) {
        if (!context.Workspaces.Home.Catalog.PerAvdOptions.contains(avdName)) {
            return;
        }

        const std::string configPath = App::GetOptionsConfigPath(avdName);
        Core::SaveOptionsToFile(configPath, context.Workspaces.Home.Catalog.PerAvdOptions[avdName]);
    }

    std::vector<Core::EmulatorOption> &GetDefaultAvdOptions(Context &context) {
        if (context.Workspaces.Home.Catalog.SelectedAvd >= 0 && context.Workspaces.Home.Catalog.SelectedAvd < context.Workspaces.Home.Catalog.Avds.size()) {
            const auto &avd = context.Workspaces.Home.Catalog.Avds.at(context.Workspaces.Home.Catalog.SelectedAvd);
            const std::string &avdName = avd.Name;

            if (!context.Workspaces.Home.Catalog.PerAvdOptions.contains(avdName)) {
                LoadAvdOptions(context, avdName);
            }
            return context.Workspaces.Home.Catalog.PerAvdOptions[avdName];
        }

        // Fallback
        static std::vector<Core::EmulatorOption> defaultOptions = Core::GetEmulatorOptions();
        return defaultOptions;
    }
}

//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef COREDECK_APPLICATION_H
#define COREDECK_APPLICATION_H

#include "context.h"
#include "../core/app_settings_types.h"
#include "../core/version_check.h"

#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace CoreDeck {
    class Application {
    public:
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;
        Application(Application &&) = delete;
        Application &operator=(Application &&) = delete;

        explicit Application();

        ~Application();

        int Run();

    private:
        void m_Build();

        bool m_InitPlatform();

        bool m_CreateMainWindow();

        void m_InitImGui();

        void m_LoadFonts() const;

        void m_ApplyDpiScale();

        void m_SetupCallbacks();

        void m_RunLoop();

        void m_Shutdown();

        void m_PollUpdateCheckIfNeeded();

        void m_ApplyNativeWindowTitle();

        Context m_Context;
        GLFWwindow *m_Window = nullptr;
        bool m_GlfwInitialized = false;
        bool m_ImGuiContextCreated = false;
        bool m_ImGuiBackendsInitialized = false;

        // Scale applied to ImGui style/sizes (1.0 on macOS, native on Win/Linux).
        float m_DpiScale = 1.0F;

        // Multiplier used when rasterizing fonts (matches the framebuffer pixel density).
        float m_FontPixelScale = 1.0F;

        std::string m_AppliedWindowPage;
        std::future<std::optional<RemoteRelease>> m_UpdateCheckFuture;
        bool m_AutoUpdateCheckStarted = false;
        bool m_UpdateCheckWasManual = false;
    };

    void ApplyWindowChrome(GLFWwindow *window);

    AppSettings CaptureAppSettingsFromContext(const Context &context);

    void ApplyAppSettingsToContext(Context &context, const AppSettings &settings);

    void PersistAppSettings(const Context &context);

    struct AvdCatalogSnapshot {
        std::vector<std::string> Names;
        std::vector<AvdInfo> Avds;
        std::unordered_map<std::string, std::vector<EmulatorOption>> Options;
    };

    AvdCatalogSnapshot LoadAvdCatalog(const SdkInfo &sdk);

    void ApplyAvdCatalog(Context &context, AvdCatalogSnapshot snapshot);

    void RefreshAvds(Context &context);

    void RequestAvdListRefresh(Context &context);

    void PollAvdListRefresh(Context &context);

    // Drops a list refresh that is already running so a newer catalog can replace it.
    void SupersedeAvdListRefresh();

    void LoadAvdOptions(Context &context, const std::string &avdName);

    void SaveAvdOptions(Context &context, const std::string &avdName);

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context);
}

#endif // COREDECK_APPLICATION_H

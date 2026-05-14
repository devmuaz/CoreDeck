//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include "context.h"
#include "../core/app_settings_types.h"
#include "../core/version_check.h"

#include <future>
#include <optional>
#include <string>

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

        Context m_Context;
        GLFWwindow *m_Window = nullptr;
        bool m_GlfwInitialized = false;
        bool m_ImGuiContextCreated = false;
        bool m_ImGuiBackendsInitialized = false;
        float m_DpiScale = 1.0F;       // Scale applied to ImGui style/sizes (1.0 on macOS, native on Win/Linux).
        float m_FontPixelScale = 1.0F; // Multiplier used when rasterizing fonts (matches the framebuffer pixel density).
        std::future<std::optional<RemoteRelease>> m_UpdateCheckFuture;
        bool m_AutoUpdateCheckStarted = false;
        bool m_UpdateCheckWasManual = false;
    };

    AppSettings CaptureAppSettingsFromContext(const Context &context);

    void ApplyAppSettingsToContext(Context &context, const AppSettings &settings);

    void PersistAppSettings(const Context &context);

    void RefreshAvds(Context &context);

    void LoadAvdOptions(Context &context, const std::string &avdName);

    void SaveAvdOptions(Context &context, const std::string &avdName);

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context);
}

#endif // EMU_LAUNCHER_RENDERER_H

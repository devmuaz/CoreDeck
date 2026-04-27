//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include "context.h"
#include "../core/app_settings_types.h"

#include <future>
#include <optional>
#include <string>

struct GLFWwindow;

namespace CoreDeck {
    class Application {
    public:
        explicit Application();

        ~Application();

        Application(const Application &) = delete;

        Application &operator=(const Application &) = delete;

        int Run();

    private:
        void Build();

        bool InitPlatform();

        bool CreateMainWindow();

        void InitImGui();

        static void LoadFonts();

        void SetupCallbacks();

        void RunLoop();

        void Shutdown();

        void PollUpdateCheckIfNeeded();

        Context m_Context;
        GLFWwindow *m_Window = nullptr;
        bool m_GlfwInitialized = false;
        bool m_ImGuiContextCreated = false;
        bool m_ImGuiBackendsInitialized = false;
        std::future<std::optional<std::string> > m_UpdateCheckFuture;
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

#endif //EMU_LAUNCHER_RENDERER_H

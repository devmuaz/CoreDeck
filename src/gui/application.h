//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include "context.h"
#include "../core/app_settings_types.h"

struct GLFWwindow;

namespace CoreDeck {
    class Application {
    public:
        Application();

        void Build();

        void SetMainWindow(GLFWwindow *window);

    private:
        Context m_Context;
    };

    void RefreshAvds(Context &context);

    void LoadAvdOptions(Context &context, const std::string &avdName);

    void SaveAvdOptions(Context &context, const std::string &avdName);

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context);

    AppSettings CaptureAppSettingsFromContext(const Context &context);

    void ApplyAppSettingsToContext(Context &context, const AppSettings &settings);

    void PersistAppSettings(const Context &context);
}

#endif //EMU_LAUNCHER_RENDERER_H

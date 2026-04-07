//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include "avd_info.h"
#include "emulator.h"
#include "options.h"
#include "sdk.h"

namespace Emu {
    class Application {
    public:
        Application();

        void Build();

    private:
        SdkInfo m_Sdk;
        std::vector<std::string> m_AvdNames;
        std::vector<AvdInfo> m_Avds;
        int m_SelectedAvd = -1;
        std::vector<EmulatorOption> m_Options;
        EmulatorManager m_Manager;
        bool m_AutoScroll = true;

        void m_RefreshAvds();

        void m_BuildOptionsPanel();

        void m_BuildAvdListPanel();

        void m_BuildAvdDetailsPanel();

        void m_BuildLogPanel();
    };
}

#endif //EMU_LAUNCHER_RENDERER_H

//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include "avd.h"
#include "emulator.h"
#include "options.h"
#include "sdk.h"
#include <unordered_map>

namespace CoreDeck {
    class Application {
    public:
        Application();

        void Build();

    private:
        SdkInfo m_Sdk;
        std::vector<std::string> m_AvdNames;
        std::vector<AvdInfo> m_Avds;
        int m_SelectedAvd = -1;
        int m_PreviousSelectedAvd = -1;
        std::unordered_map<std::string, std::vector<EmulatorOption> > m_PerAvdOptions;
        std::unordered_map<std::string, std::string> m_PerAvdLogSearch;
        EmulatorManager m_Manager;
        bool m_AutoScroll = true;
        bool m_ShowAboutDialog = false;
        bool m_ShowDeleteDialog = false;

        void m_RefreshAvds();

        void m_BuildOptionsPanel();

        void m_BuildAvdListPanel();

        void m_BuildAvdDetailsPanel();

        void m_BuildLogPanel();

        void m_BuildMenuBar();

        void m_BuildAboutDialog();

        void m_BuildDeleteDialog();

        void m_LoadAvdOptions(const std::string &avdName);

        void m_SaveAvdOptions(const std::string &avdName);

        std::vector<EmulatorOption> &m_GetCurrentAvdOptions();
    };
}

#endif //EMU_LAUNCHER_RENDERER_H

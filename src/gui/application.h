//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_RENDERER_H
#define EMU_LAUNCHER_RENDERER_H

#include <future>
#include <unordered_map>
#include <vector>

#include "../core/sdk.h"
#include "../core/avd.h"
#include "../core/emulator.h"
#include "../core/options.h"

namespace CoreDeck {
    enum class Screen {
        Onboarding,
        Main
    };

    struct Context {
        SdkInfo Sdk;
        EmulatorManager Manager;
        Screen CurrentScreen = Screen::Main;
        std::vector<std::string> AvdNames;
        std::vector<AvdInfo> Avds;
        int SelectedAvd = -1;
        int PreviousSelectedAvd = -1;
        std::unordered_map<std::string, std::vector<EmulatorOption> > PerAvdOptions;
        std::unordered_map<std::string, std::string> PerAvdLogSearch;
        bool AutoScroll = true;
        bool ShowAboutDialog = false;
        bool ShowDeleteDialog = false;
        bool ShowCreateDialog = false;

        // Create dialog data
        std::vector<SystemImage> SystemImages;
        std::vector<DeviceProfile> DeviceProfiles;
        CreateAvdParams CreateParams;
        int SelectedSystemImage = 0;
        int SelectedDevice = 0;
        int SelectedGpuMode = 0;
        std::atomic<bool> CreateDataLoading{false};
        std::atomic<bool> CreateDataReady{false};
        std::future<void> CreateDataFuture;

        // Async operation state (shared)
        std::atomic<bool> AsyncBusy{false};
        std::future<void> AsyncFuture;

        explicit Context(SdkInfo sdk) : Sdk(std::move(sdk)), Manager(Sdk) {
        }
    };

    class Application {
    public:
        Application();

        void Build();

    private:
        Context m_Context;
    };

    void RefreshAvds(Context &context);

    void LoadAvdOptions(Context &context, const std::string &avdName);

    void SaveAvdOptions(Context &context, const std::string &avdName);

    std::vector<EmulatorOption> &GetDefaultAvdOptions(Context &context);
}

#endif //EMU_LAUNCHER_RENDERER_H

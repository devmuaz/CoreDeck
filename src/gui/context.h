//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_CONTEXT_H
#define COREDECK_CONTEXT_H

#include <atomic>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/avd.h"
#include "../core/emulator.h"
#include "../core/options.h"
#include "../core/sdk.h"

struct GLFWwindow;

namespace CoreDeck {
    enum class Screen {
        Onboarding,
        Main,
    };

    struct Context {
        struct Host {
            SdkInfo Sdk;
            EmulatorManager Manager;

            explicit Host(SdkInfo sdk) : Sdk(std::move(sdk)), Manager(Sdk) {
            }
        } Host;

        struct Flow {
            Screen CurrentScreen = Screen::Main;
        } Flow;

        struct Catalog {
            std::vector<std::string> AvdNames;
            std::vector<AvdInfo> Avds;
            int SelectedAvd = -1;
            int PreviousSelectedAvd = -1;
            std::unordered_map<std::string, std::vector<EmulatorOption> > PerAvdOptions;
        } Catalog;

        struct Logs {
            std::unordered_map<std::string, std::string> PerAvdLogSearch;
            bool AutoScroll = true;
        } Logs;

        struct Prefs {
            bool ConfirmBeforeDeleteAvd = true;
        } Prefs;

        struct UI {
            bool ShowAboutDialog = false;
            bool ShowDeleteAvdDialog = false;
            bool ShowCreateAvdDialog = false;
            bool ShowPreferences = false;
            bool ShowAvdListPanel = true;
            bool ShowOptionsPanel = true;
            bool ShowDetailsPanel = true;
            bool ShowLogPanel = true;
            GLFWwindow *MainWindow = nullptr;
            bool HideInvalidSdkPathBanner = false;
        } UI;

        struct CreateAvdWork {
            std::vector<SystemImage> SystemImages;
            std::vector<DeviceProfile> DeviceProfiles;
            CreateAvdParams CreateParams;
            int SelectedSystemImage = 0;
            int SelectedDevice = 0;
            int SelectedGpuMode = 0;

            struct {
                std::atomic<bool> Loading{false};
                std::atomic<bool> Ready{false};
                std::future<void> Future;
            } Prefetch;
        } CreateAvdWork;

        struct Jobs {
            struct {
                std::atomic<bool> Busy{false};
                std::future<void> Future;
            } AvdCreation, AvdDeletion;
        } Jobs;

        struct Updates {
            bool ShowNewVersionModal = false;
            std::string LatestVersion;
            bool ShowUpToDateModal = false;
            bool RequestManualUpdateCheck = false;
            bool UpdateCheckInFlight = false;
        } Updates;

        explicit Context(SdkInfo sdk) : Host(std::move(sdk)) {
        }
    };
}

#endif //COREDECK_CONTEXT_H

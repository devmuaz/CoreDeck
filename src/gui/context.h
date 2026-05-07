//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_CONTEXT_H
#define COREDECK_CONTEXT_H

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/avd.h"
#include "../core/emulator.h"
#include "../core/options.h"
#include "../core/sdk.h"
#include "../core/skin.h"
#include "../core/system_image.h"

struct GLFWwindow;

namespace CoreDeck {
    enum class Screen {
        Onboarding,
        Main,
    };

    enum class AvdSortMode {
        Name,
        ApiLevel,
        Device,
    };

    enum class ImageCategory {
        All,
        PhoneTablet,
        Wear,
        Tv,
        Automotive,
        Desktop,
        Xr,
        Other,
    };

    enum class DeviceCategory {
        All,
        Phone,
        Tablet,
        Wear,
        Tv,
        Automotive,
        Desktop,
        Other,
    };

    struct StorageScanResult {
        std::uintmax_t TotalAvdSize = 0;
        std::uintmax_t SystemImagesSize = 0;
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
            std::unordered_map<std::string, std::vector<EmulatorOption>> PerAvdOptions;

            char SearchFilter[128] = {};
            AvdSortMode SortMode = AvdSortMode::Name;
            bool SortAscending = true;
            std::vector<int> FilteredIndices;
        } Catalog;

        struct LogViewState {
            std::string Search;
            int ActiveMatchIndex = 0;
            bool UseRegex = false;
        };

        struct Logs {
            std::unordered_map<std::string, LogViewState> PerAvdView;
            bool AutoScroll = true;
            bool PendingScroll = false;
            bool PendingFocus = false;
            int PendingSyncFrames = 0;
        } Logs;

        struct Prefs {
            bool ConfirmBeforeDeleteAvd = true;
        } Prefs;

        struct UI {
            bool ShowAboutDialog = false;
            bool ShowDeleteAvdDialog = false;
            bool ShowCreateAvdDialog = false;
            bool ShowDeviceProfileDialog = false;
            bool ShowSkinDialog = false;
            bool ShowInstallImageDialog = false;
            bool ReopenCreateAvdOnInstallClose = false;
            bool ShowPreferences = false;
            bool ShowStorageDialog = false;
            bool ShowWipeDataDialog = false;
            bool ShowAvdListPanel = true;
            bool ShowOptionsPanel = true;
            bool ShowDetailsPanel = true;
            bool ShowLogPanel = true;
            GLFWwindow *MainWindow = nullptr;
            bool HideInvalidSdkPathBanner = false;
        } UI;

        struct AvdCreationWork {
            std::vector<SystemImage> SystemImages;
            std::vector<DeviceProfile> DeviceProfiles;
            std::vector<Skin> Skins;
            AvdCreationData CreationData;
            int SelectedSystemImage = 0;
            int SelectedDevice = 0;
            int PendingSelectedDevice = 0;
            int SelectedSkin = 0; // 0 = "No skin"
            int PendingSelectedSkin = 0;
            char SkinSearchFilter[128] = {};
            DeviceCategory SelectedDeviceCategory = DeviceCategory::Phone;
            char DeviceSearchFilter[128] = {};
            int SelectedGpuMode = 0;
            bool NameAutoFilled = true;
            bool DisplayNameAutoFilled = true;
            bool SkinAutoFilled = true;
            int LastDeviceForSkinAuto = -1;

            struct {
                std::atomic<bool> Loading{false};
                std::atomic<bool> Ready{false};
                std::future<void> Future;
            } Prefetch;

            struct {
                std::atomic<bool> Busy{false};
                std::future<bool> Future;
            } SystemImageRemoval;
        } AvdCreationWork;

        struct ImageInstallationWork {
            std::vector<RemoteSystemImage> RemoteImages;
            int SelectedImage = -1;
            ImageCategory SelectedCategory = ImageCategory::PhoneTablet;
            char SearchFilter[128] = {};

            struct {
                std::atomic<bool> Loading{false};
                std::atomic<bool> Ready{false};
                std::future<void> Future;
            } Prefetch;

            std::atomic<bool> Installing{false};
            std::shared_ptr<InstallProgressData> Progress;
            std::future<bool> InstallFuture;
            bool AwaitingLicenseConsent = false;
            std::atomic<bool> LicenseBusy{false};
            std::future<LicenseStatus> LicenseCheckFuture;
            std::future<bool> LicenseAcceptFuture;
            std::string PendingPackagePath;
            std::string LicenseError;
        } ImageInstallationWork;

        struct Jobs {
            struct {
                std::atomic<bool> Busy{false};
                std::future<void> Future;
            } AvdCreation, AvdDeletion, AvdWipe;
        } Jobs;

        struct DiskUsage {
            std::unordered_map<std::string, std::uintmax_t> PerAvdCache;
            StorageScanResult LastScan;
            std::atomic<bool> Loading{false};
            bool Ready = false;
            std::future<StorageScanResult> Future;
        } DiskUsage;

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

#endif // COREDECK_CONTEXT_H

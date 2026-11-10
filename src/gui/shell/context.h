//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_GUI_SHELL_CONTEXT_H
#define COREDECK_GUI_SHELL_CONTEXT_H

#include <cstdint>
#include <string>
#include <utility>

#include "app/app_settings_types.h"
#include "core/adb/adb_service.h"
#include "core/avd/emulator.h"
#include "core/sdk/jdk.h"
#include "core/sdk/sdk.h"
#include "gui/adb/adb_state.h"
#include "gui/apk/apk_analyzer_state.h"
#include "gui/apk/keytool_state.h"
#include "gui/apk/signer_state.h"
#include "gui/home/create_avd_state.h"
#include "gui/home/home_state.h"
#include "gui/home/install_image_state.h"
#include "gui/sdk/health_check_state.h"
#include "gui/sdk/sdk_bootstrap_state.h"

struct GLFWwindow;

namespace CoreDeck::Gui {
    enum class Screen : uint8_t {
        Onboarding,
        Main,
    };

    enum class WorkspacePage : uint8_t {
        Home,
        Adb,
        ApkAnalyzer,
        ApkSigner,
        Keytool,
        JarSigner,
    };

    struct DialogOpen {
        bool Open = false;
    };

    struct Context {
        struct Host {
            Core::SdkInfo Sdk;
            Core::JdkInfo Jdk;
            Core::EmulatorManager Manager;
            Core::AdbService Adb;

            explicit Host(Core::SdkInfo sdk) : Sdk(std::move(sdk)), Manager(Sdk), Adb(Sdk) {
            }

            void SyncSdk() {
                Manager.SetSdk(Sdk);
                Adb.SetSdk(Sdk);
            }
        } Host;

        struct Prefs {
            bool AutoScroll = true;
            bool ConfirmBeforeDeleteAvd = true;
            bool CrashReportingEnabled = true;
            bool CheckForUpdatesOnLaunch = true;
            bool IncludeBetaReleases = false;
            App::ThemePreference Theme = App::ThemePreference::System;
            std::string Language;
        } Prefs;

        struct Shell {
            Screen CurrentScreen = Screen::Main;
            WorkspacePage Workspace = WorkspacePage::Home;
            GLFWwindow *MainWindow = nullptr;
            std::string NativeWindowPage;
            bool AvdListPanel = true;
            bool OptionsPanel = true;
            bool DetailsPanel = true;
            bool LogPanel = true;
            bool HideHealthCheckBanner = false;
            SdkBootstrapState SdkBootstrap;
        } Shell;

        struct Workspaces {
            struct Home {
                Gui::Catalog Catalog;
                Gui::Logs Logs;
                Gui::Jobs Jobs;
                Gui::DiskUsage DiskUsage;
            } Home;

            AdbState Adb;
            ApkAnalyzerState ApkAnalyzer;
            ApkSignerState ApkSigner;
            JarSignerState JarSigner;
            KeytoolState Keytool;
        } Workspaces;

        struct Dialogs {
            DialogOpen About;
            DialogOpen DeleteAvd;

            struct CreateAvd {
                bool Open = false;
                AvdCreationState State;
            } CreateAvd;

            DialogOpen DeviceProfile;
            DialogOpen Skin;

            struct InstallImage {
                bool Open = false;
                bool ReopenCreateAvdOnClose = false;
                ImageInstallationState State;
            } InstallImage;

            DialogOpen Storage;
            DialogOpen WipeData;

            struct HealthCheck {
                bool Open = false;
                HealthCheckState State;
            } HealthCheck;

            struct AcceptLicenses {
                bool Open = false;
                AcceptLicensesState State;
                LicenseNotice Notice;
            } AcceptLicenses;

            struct Preferences {
                bool Open = false;
                bool OpenToJava = false;
            } Preferences;

            struct Update {
                bool Open = false;
                std::string LatestVersion;
                std::string LatestNotes;
                bool UpToDateOpen = false;
                bool RequestManualUpdateCheck = false;
                bool UpdateCheckInFlight = false;
            } Update;
        } Dialogs;

        explicit Context(Core::SdkInfo sdk) : Host(std::move(sdk)) {
        }
    };

    inline void SetNativeWindowPage(Context &context, const char *page) {
        context.Shell.NativeWindowPage = page != nullptr ? page : "";
    }

    inline void OpenAboutDialog(Context &context) {
        context.Dialogs.About.Open = true;
    }

    inline void OpenDeleteAvdDialog(Context &context) {
        context.Dialogs.DeleteAvd.Open = true;
    }

    inline void OpenDeviceProfileDialog(Context &context) {
        context.Dialogs.DeviceProfile.Open = true;
    }

    inline void OpenSkinDialog(Context &context) {
        context.Dialogs.Skin.Open = true;
    }

    inline void OpenStorageDialog(Context &context) {
        context.Dialogs.Storage.Open = true;
    }

    inline void OpenWipeDataDialog(Context &context) {
        context.Dialogs.WipeData.Open = true;
    }

    inline void OpenPreferencesDialog(Context &context, const bool javaSection = false) {
        context.Dialogs.Preferences.Open = true;
        if (javaSection) {
            context.Dialogs.Preferences.OpenToJava = true;
        }
    }

    inline void OpenUpdateDialog(Context &context) {
        context.Dialogs.Update.Open = true;
    }

    inline void OpenUpToDateDialog(Context &context) {
        context.Dialogs.Update.UpToDateOpen = true;
    }
}

#endif // COREDECK_GUI_SHELL_CONTEXT_H

//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#ifndef COREDECK_CONTEXT_H
#define COREDECK_CONTEXT_H

#include <cstdint>
#include <string>
#include <utility>

#include "app/app_settings_types.h"
#include "core/adb_service.h"
#include "core/emulator.h"
#include "core/jdk.h"
#include "core/sdk.h"
#include "adb_work.h"
#include "apk_analyzer_work.h"
#include "avd_creation_work.h"
#include "avd_home.h"
#include "health_work.h"
#include "image_installation_work.h"
#include "keytool_work.h"
#include "sdk_bootstrap_work.h"
#include "signer_work.h"

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

    using CatalogType = Catalog;
    using LogsType = Logs;
    using AvdCreationWorkType = AvdCreationWork;
    using ImageInstallationWorkType = ImageInstallationWork;
    using SdkBootstrapWorkType = SdkBootstrapWork;
    using HealthCheckWorkType = HealthCheckWork;
    using AcceptLicensesWorkType = AcceptLicensesWork;
    using LicenseNoticeType = LicenseNotice;
    using JobsType = Jobs;
    using DiskUsageType = DiskUsage;
    using ApkAnalyzerWorkType = ApkAnalyzerWork;
    using ApkSignerWorkType = ApkSignerWork;
    using JarSignerWorkType = JarSignerWork;
    using AdbWorkType = AdbWork;
    using KeytoolWorkType = KeytoolWork;

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

        struct Flow {
            Screen CurrentScreen = Screen::Main;
        } Flow;

        CatalogType Catalog;
        LogsType Logs;

        struct Prefs {
            bool AutoScroll = true;
            bool ConfirmBeforeDeleteAvd = true;
            bool CrashReportingEnabled = true;
            bool CheckForUpdatesOnLaunch = true;
            bool IncludeBetaReleases = false;
            App::ThemePreference Theme = App::ThemePreference::System;
            std::string Language;
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
            WorkspacePage Workspace = WorkspacePage::Home;
            bool ShowWipeDataDialog = false;
            bool ShowHealthCheckDialog = false;
            bool ShowAcceptLicensesDialog = false;
            bool ShowAvdListPanel = true;
            bool ShowOptionsPanel = true;
            bool ShowDetailsPanel = true;
            bool ShowLogPanel = true;
            GLFWwindow *MainWindow = nullptr;
            bool HideHealthCheckBanner = false;
            bool OpenPreferencesToJava = false;
            std::string NativeWindowPage;
        } UI;

        AvdCreationWorkType AvdCreationWork;
        ImageInstallationWorkType ImageInstallationWork;
        SdkBootstrapWorkType SdkBootstrapWork;
        HealthCheckWorkType HealthCheckWork;
        AcceptLicensesWorkType AcceptLicensesWork;
        LicenseNoticeType LicenseNotice;
        JobsType Jobs;
        DiskUsageType DiskUsage;
        ApkAnalyzerWorkType ApkAnalyzerWork;
        ApkSignerWorkType ApkSignerWork;
        JarSignerWorkType JarSignerWork;
        AdbWorkType AdbWork;
        KeytoolWorkType KeytoolWork;

        struct Updates {
            bool ShowNewVersionModal = false;
            std::string LatestVersion;
            std::string LatestNotes;
            bool ShowUpToDateModal = false;
            bool RequestManualUpdateCheck = false;
            bool UpdateCheckInFlight = false;
        } Updates;

        explicit Context(Core::SdkInfo sdk) : Host(std::move(sdk)) {
        }
    };

    inline void SetNativeWindowPage(Context &context, const char *page) {
        context.UI.NativeWindowPage = page != nullptr ? page : "";
    }
}

#endif // COREDECK_CONTEXT_H

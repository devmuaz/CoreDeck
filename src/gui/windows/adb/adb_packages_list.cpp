//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "adb_packages_list.h"

#include "adb_session.h"

#include <optional>

#include "platform/i18n.h"
#include "platform/utilities.h"

namespace CoreDeck::Gui::AdbWindow {
    namespace {
        void CancelPackageList(AdbDeviceSession &session) {
            session.PackageListRestart = false;
            if (session.Busy.load() && session.Task == AdbTask::ListPackages) {
                session.Cancel.store(true);
            }
        }

        void QueuePackageList(Context &context, AdbDeviceSession &session) {
            session.PackageListRestart = false;
            session.PackageListError.clear();
            const Core::AdbPackageListScope scope = session.PackageScope;
            StartTask(
                context,
                session,
                AdbTask::ListPackages,
                [scope](const Core::SdkInfo &sdk, const std::string &serial, const std::atomic<bool> *cancel) {
                    return Core::RunAdb(sdk, Core::ListAdbPackagesCommand(scope), serial, cancel);
                }
            );
        }

        void SelectPackageScope(Context &context, AdbDeviceSession &session, const Core::AdbPackageListScope scope) {
            if (scope == session.PackageScope) {
                return;
            }
            session.PackageScope = scope;
            if (session.Busy.load() && session.Task == AdbTask::ListPackages) {
                session.PackageListRestart = true;
                session.Cancel.store(true);
                return;
            }
            QueuePackageList(context, session);
        }

        std::optional<std::string> PackageRows(const AdbDeviceSession &session) {
            const std::string filter = session.PackageFilter;
            std::optional<std::string> picked;
            int shown = 0;
            for (int index = 0; std::cmp_less(index, session.Packages.size()); ++index) {
                const std::string &name = session.Packages.at(static_cast<std::size_t>(index));
                if (!Platform::ContainsIgnoreCase(name, filter)) {
                    continue;
                }
                ++shown;
                ImGui::PushID(index);
                if (SelectableItem(name.c_str(), name == session.PackageName) && !picked) {
                    picked = name;
                }
                ImGui::PopID();
            }
            if (shown == 0) {
                const bool filtered = session.PackageFilter[0] != '\0';
                CenteredMessage(
                    Platform::Tr("No Packages"),
                    filtered ? Platform::Tr("No packages match the search.") : Platform::Tr("This device reported no packages.")
                );
            }
            return picked;
        }

        void PackageListBody(AdbDeviceSession const &session, const bool loading, std::optional<std::string> &picked) {
            StyleColor colors;
            StyleVar rounding;
            colors.Push(ImGuiCol_ChildBg, HexColor(Colors::SURFACE1));
            colors.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
            rounding.Push(ImGuiStyleVar_ChildRounding, ImGui::GetStyle().FrameRounding);
            rounding.Push(ImGuiStyleVar_ChildBorderSize, 1.0F);
            ImGui::BeginChild("##AdbPackageList", ImVec2(0.0F, Eh(16.0F)), ImGuiChildFlags_Borders);
            {
                const LoadingOverlay overlay(loading, Platform::Tr("Loading packages..."));
                if (!loading && session.PackageListError.empty()) {
                    picked = PackageRows(session);
                } else if (!loading) {
                    const char *title = Platform::Tr("Could not list packages.");
                    const char *detail = session.PackageListError == title ? nullptr : session.PackageListError.c_str();
                    CenteredMessage(title, detail, Colors::NEGATIVE);
                }
            }
            ImGui::EndChild();
        }

    }

    void OpenPackageList(Context &context, AdbDeviceSession &session) {
        session.PackageFilter[0] = '\0';
        session.Packages.clear();
        session.PackageScope = Core::AdbPackageListScope::All;
        session.ShowPackageDialog = true;
        QueuePackageList(context, session);
    }

    void BrowsePackages(Context &context) {
        auto &session = SelectedSession(context.AdbWork);
        const std::string title = Platform::TrWindow("Installed Packages", "AdbPackages");
        if (!session.ShowPackageDialog && !ImGui::IsPopupOpen(title.c_str())) {
            return;
        }

        const bool requested = session.ShowPackageDialog;
        if (!BeginCenteredModal(title.c_str(), &session.ShowPackageDialog, EmV(46.0F, 24.0F), WINDOW_AUTO_RESIZE_FLAGS)) {
            return;
        }
        if (!session.ShowPackageDialog) {
            if (requested) {
                CancelPackageList(session);
            }
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        if (session.PackageListRestart && !session.Busy.load()) {
            QueuePackageList(context, session);
        }

        const char *scopes[] = {
            Platform::Tr("All"),
            Platform::Tr("Third-party only"),
            Platform::Tr("System apps"),
        };
        static_assert(IM_ARRAYSIZE(scopes) == static_cast<int>(Core::AdbPackageListScope::System) + 1);
        int scope = static_cast<int>(session.PackageScope);
        const int currentScope = scope;
        if (CategoryChipRow(scopes, IM_ARRAYSIZE(scopes), scope) && scope != currentScope) {
            SelectPackageScope(context, session, static_cast<Core::AdbPackageListScope>(scope));
        }
        ImGui::Spacing();
        SearchField("##AdbPackageSearch", Platform::Tr("Search packages..."), session.PackageFilter, sizeof(session.PackageFilter));
        ImGui::Spacing();
        const bool loading = session.Busy.load() && session.Task == AdbTask::ListPackages;
        std::optional<std::string> picked;
        PackageListBody(session, loading, picked);
        ImGui::Spacing();
        const float buttonW = EqualButtonWidth(2);
        if (PrimaryButton(Platform::Tr("Refresh"), !loading, ImVec2(buttonW, 0.0F))) {
            QueuePackageList(context, session);
        }
        ImGui::SameLine();
        if (PrimaryButton(Platform::Tr("Cancel"), true, ImVec2(buttonW, 0.0F))) {
            CancelPackageList(session);
            session.ShowPackageDialog = false;
            ImGui::CloseCurrentPopup();
        }
        if (picked) {
            CopyToBuffer(session.PackageName, sizeof(session.PackageName), *picked);
            SetTabStatus(
                session,
                AdbWorkspaceTab::PackagesFiles,
                StatusBarTone::Success,
                Platform::TrFormat("Selected {0}.", *picked)
            );
            session.ShowPackageDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

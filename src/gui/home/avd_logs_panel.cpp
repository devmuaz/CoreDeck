//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "gui/home/avd_logs_panel.h"
#include "gui/shell/context.h"
#include "gui/widgets/log_viewer.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    void BuildAvdLogsPanel(Context &context) {
        if (!context.Shell.LogPanel) {
            return;
        }

        const std::string title = Platform::TrWindow("Output Log", "Output Log");
        ImGui::Begin(title.c_str());

        std::shared_ptr<Platform::LogBuffer> log;
        std::string avdName;
        bool selected = false;
        if (context.Workspaces.Home.Catalog.SelectedAvd >= 0) {
            selected = true;
            avdName = context.Workspaces.Home.Catalog.Avds.at(context.Workspaces.Home.Catalog.SelectedAvd).Name;
            log = context.Host.Manager.GetLog(avdName);
        }

        LogViewerState scratch;
        LogViewerState &state = selected ? context.Workspaces.Home.Logs.PerAvdView[avdName] : scratch;
        const bool running = selected && context.Host.Manager.IsRunning(avdName);
        const std::string placeholder =
            !selected ? Platform::Tr("Select an AVD to view logs")
                      : Platform::TrFormat("Run the \"{0}\" AVD to view logs", avdName);
        LogViewer(
            "AvdOutput",
            log,
            state,
            {
                .EmptyText = placeholder.c_str(),
                .AutoScroll = context.Prefs.AutoScroll,
                .AllowPause = true,
                .Live = running,
            }
        );

        ImGui::End();
    }
}

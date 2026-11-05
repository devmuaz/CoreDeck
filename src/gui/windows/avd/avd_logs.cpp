//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "avd_logs.h"
#include "../../context/context.h"
#include "../../widgets/log_viewer.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    void BuildAvdLogsWindow(Context &context) {
        if (!context.UI.ShowLogPanel) {
            return;
        }

        const std::string title = Platform::TrWindow("Output Log", "Output Log");
        ImGui::Begin(title.c_str());

        std::shared_ptr<Platform::LogBuffer> log;
        std::string avdName;
        bool selected = false;
        if (context.Catalog.SelectedAvd >= 0) {
            selected = true;
            avdName = context.Catalog.Avds.at(context.Catalog.SelectedAvd).Name;
            log = context.Host.Manager.GetLog(avdName);
        }

        LogViewerState scratch;
        LogViewerState &state = selected ? context.Logs.PerAvdView[avdName] : scratch;
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

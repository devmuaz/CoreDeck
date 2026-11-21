#include <cstdlib>

#include "app/app_config.h"
#include "app/crash_reporter.h"
#include "gui/shell/application.h"

int main() {
    const CoreDeck::App::AppConfig config = CoreDeck::App::LoadAppConfig();
    const bool isCrashReportingEnabled = config.Preferences->CrashReportingEnabled.value();

    CoreDeck::App::CrashReporter::Init(isCrashReportingEnabled);
    int code = 0;
    {
        CoreDeck::Gui::Application app;
        code = app.Run();
    }
    CoreDeck::App::CrashReporter::Shutdown(isCrashReportingEnabled);
    std::_Exit(code);
}
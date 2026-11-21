#include "app/app_config.h"
#include "app/crash_reporter.h"
#include "gui/shell/application.h"

int main() {
    const CoreDeck::App::AppConfig config = CoreDeck::App::LoadAppConfig();
    const bool isCrashReportingEnabled = config.Preferences->CrashReportingEnabled.value();

    CoreDeck::App::CrashReporter::Init(isCrashReportingEnabled);
    CoreDeck::Gui::Application app;
    const int code = app.Run();
    CoreDeck::App::CrashReporter::Shutdown(isCrashReportingEnabled);
    return code;
}
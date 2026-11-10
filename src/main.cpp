#include "app/app_settings.h"
#include "app/app_settings_types.h"
#include "app/crash_reporter.h"
#include "gui/shell/application.h"

int main() {
    const CoreDeck::App::AppSettings settings = CoreDeck::App::LoadAppSettings();
    const bool isCrashReportingEnabled = settings.CrashReportingEnabled;

    CoreDeck::App::CrashReporter::Init(isCrashReportingEnabled);
    CoreDeck::Gui::Application app;
    const int code = app.Run();
    CoreDeck::App::CrashReporter::Shutdown(isCrashReportingEnabled);
    return code;
}
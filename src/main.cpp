#include "core/app_settings_types.h"
#include "gui/application.h"
#include "core/crash_reporter.h"
#include "core/app_settings.h"

int main() {
    const CoreDeck::AppSettings settings = CoreDeck::LoadAppSettings();
    const bool isCrashReportingEnabled = settings.CrashReportingEnabled;

    CoreDeck::CrashReporter::Init(isCrashReportingEnabled);
    CoreDeck::Application app;
    const int code = app.Run();
    CoreDeck::CrashReporter::Shutdown(isCrashReportingEnabled);
    return code;
}
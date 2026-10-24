#include "core/app_settings_types.h"
#include "gui/application.h"
#include "core/crash_reporter.h"
#include "core/app_settings.h"

int main() {
    const CoreDeck::Core::AppSettings settings = CoreDeck::Core::LoadAppSettings();
    const bool isCrashReportingEnabled = settings.CrashReportingEnabled;

    CoreDeck::Core::CrashReporter::Init(isCrashReportingEnabled);
    CoreDeck::Gui::Application app;
    const int code = app.Run();
    CoreDeck::Core::CrashReporter::Shutdown(isCrashReportingEnabled);
    return code;
}
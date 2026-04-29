#include "gui/application.h"
#include "core/crash_reporter.h"

int main() {
    CoreDeck::CrashReporter::Init();
    CoreDeck::Application app;
    const int code = app.Run();
    CoreDeck::CrashReporter::Shutdown();
    return code;
}
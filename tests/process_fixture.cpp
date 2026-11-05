#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {
    bool WriteText(FILE *file, const char *text) {
        return std::fputs(text, file) >= 0 && std::fflush(file) == 0;
    }
}

int main(const int argc, char *const *argv) {
    if (argc < 2) {
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "-s" && argc > 3 && std::string(argv[3]) == "logcat") {
        if (!WriteText(stdout, "11-01 04:00:00.000  100  100 I CoreDeck: first\n") ||
            !WriteText(stdout, "11-01 04:00:00.001  100  100 W CoreDeck: second\n") ||
            !WriteText(stderr, "logcat diagnostic")) {
            return 1;
        }
        return 0;
    }
    if (mode == "emit") {
        const char bytes[] = {'A', '\0', 'B'};
        if (std::fwrite(bytes, 1, sizeof(bytes), stdout) != sizeof(bytes) || std::fflush(stdout) != 0 ||
            !WriteText(stderr, "stderr-line")) {
            return 1;
        }
        return 7;
    }
    if (mode == "env") {
        const char *value = std::getenv("COREDECK_PROCESS_TEST"); // NOLINT(concurrency-mt-unsafe)
        if (value != nullptr && !WriteText(stdout, value)) {
            return 1;
        }
        return 0;
    }
    if (mode == "sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    return 3;
}

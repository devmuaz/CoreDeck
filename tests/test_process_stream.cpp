#include <array>
#include <chrono>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "platform/process.h"

using namespace CoreDeck;

namespace {
    std::string ReadAll(int &fd) {
        std::string output;
        std::array<char, 64> buffer{};
        for (int attempt = 0; attempt < 200 && fd >= 0; ++attempt) {
            const Platform::PipeReadResult read = Platform::ReadProcessPipe(fd, buffer.data(), buffer.size());
            if (read.Status == Platform::PipeReadStatus::Data) {
                output.append(buffer.data(), read.Size);
            } else if (read.Status == Platform::PipeReadStatus::Closed ||
                       read.Status == Platform::PipeReadStatus::Error) {
                Platform::CloseProcessPipe(fd);
                fd = -1;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        return output;
    }
}

TEST_CASE("Piped process preserves bytes and separates stderr", "[process]") {
    Platform::PipedProcess process = Platform::SpawnProcessWithPipes(COREDECK_PROCESS_FIXTURE, {"emit"});
    REQUIRE(process);

    const std::string output = ReadAll(process.StdoutFd);
    const std::string error = ReadAll(process.StderrFd);
    const Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 2000);
    Platform::ClosePipedProcess(process);

    REQUIRE(output.size() == 3);
    REQUIRE(output.at(0) == 'A');
    REQUIRE(output.at(1) == '\0');
    REQUIRE(output.at(2) == 'B');
    REQUIRE(error == "stderr-line");
    REQUIRE(exit.Exited);
    REQUIRE(exit.ExitCode == 7);
}

TEST_CASE("Piped process receives extra environment variables", "[process]") {
    Platform::PipedProcess process = Platform::SpawnProcessWithPipes(
        COREDECK_PROCESS_FIXTURE,
        {"env"},
        {{.Name = "COREDECK_PROCESS_TEST", .Value = "visible"}}
    );
    REQUIRE(process);

    const std::string output = ReadAll(process.StdoutFd);
    ReadAll(process.StderrFd);
    const Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 2000);
    Platform::ClosePipedProcess(process);

    REQUIRE(output == "visible");
    REQUIRE(exit.ExitCode == 0);
}

TEST_CASE("InterruptProcess reaps a running process without a grace period", "[process]") {
    Platform::PipedProcess process = Platform::SpawnProcessWithPipes(COREDECK_PROCESS_FIXTURE, {"sleep"});
    REQUIRE(process);

    const auto started = std::chrono::steady_clock::now();
    REQUIRE(Platform::InterruptProcess(process.Pid));
    const Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 1000);
    Platform::ClosePipedProcess(process);

    REQUIRE(exit.Exited);
    REQUIRE(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(500));
}

TEST_CASE("Piped process can be cancelled and reaped", "[process]") {
    Platform::PipedProcess process = Platform::SpawnProcessWithPipes(COREDECK_PROCESS_FIXTURE, {"sleep"});
    REQUIRE(process);

    REQUIRE(Platform::TerminateProcessTree(process.Pid));
    const Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 2000);
    Platform::ClosePipedProcess(process);

    REQUIRE(exit.Exited);
}

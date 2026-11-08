//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#include "core/adb_client.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <sstream>
#include <string_view>
#include <thread>

#include "platform/process.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        constexpr std::size_t READ_CHUNK_SIZE = 4096;

        bool DrainPipe(
            int &fd,
            std::string &target,
            std::array<char, READ_CHUNK_SIZE> &buffer,
            const std::size_t limit
        ) {
            if (fd < 0) {
                return false;
            }

            bool readData = false;
            while (true) {
                const Platform::PipeReadResult read = Platform::ReadProcessPipe(fd, buffer.data(), buffer.size());
                if (read.Status == Platform::PipeReadStatus::Data) {
                    Platform::AppendLimited(target, std::string_view(buffer.data(), read.Size), limit);
                    readData = true;
                    continue;
                }
                if (read.Status == Platform::PipeReadStatus::Closed || read.Status == Platform::PipeReadStatus::Error) {
                    Platform::CloseProcessPipe(fd);
                    fd = -1;
                }
                return readData;
            }
        }

        bool ReportContains(const std::string &output, const std::string &needle) {
            return Platform::LowerCopy(output).find(needle) != std::string::npos;
        }

        std::optional<std::string> PackageNameFromListLine(const std::string &line) {
            const std::string trimmed = Platform::Trim(line);
            constexpr std::string_view PREFIX = "package:";
            if (!trimmed.starts_with(PREFIX)) {
                return std::nullopt;
            }
            std::string name = trimmed.substr(PREFIX.size());
            if (const auto equals = name.rfind('='); equals != std::string::npos) {
                name = name.substr(equals + 1);
            }
            if (const auto space = name.find_first_of(" \t"); space != std::string::npos) {
                name.resize(space);
            }
            name = Platform::Trim(name);
            if (name.empty()) {
                return std::nullopt;
            }
            return name;
        }
    }

    std::vector<std::string> BuildAdbArguments(
        const std::string &serial,
        const std::vector<std::string> &command
    ) {
        std::vector<std::string> args;
        args.reserve(command.size() + (serial.empty() ? 0U : 2U));
        if (!serial.empty()) {
            args.emplace_back("-s");
            args.push_back(serial);
        }
        args.insert(args.end(), command.begin(), command.end());
        return args;
    }

    std::string ParseAdbVersion(const std::string &output) {
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            line = Platform::Trim(line);
            constexpr std::string_view PREFIX = "Android Debug Bridge version ";
            if (line.starts_with(PREFIX)) {
                return line.substr(PREFIX.size());
            }
        }
        return {};
    }

    bool IsValidAdbNetworkEndpoint(const std::string &endpoint) {
        if (endpoint.empty() || endpoint.find_first_of(" \t\r\n") != std::string::npos) {
            return false;
        }
        const auto separator = endpoint.rfind(':');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= endpoint.size()) {
            return false;
        }
        int port = 0;
        const std::string_view value(endpoint.data() + separator + 1, endpoint.size() - separator - 1);
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
        return error == std::errc{} && end == value.data() + value.size() && port > 0 && port <= 65535;
    }

    bool AdbConnectReportSucceeded(const std::string &output) {
        return ReportContains(output, "connected to") || ReportContains(output, "already connected");
    }

    bool AdbDisconnectReportSucceeded(const std::string &output) {
        return ReportContains(output, "disconnected");
    }

    namespace {
        bool IsPrivateIpv4(const int first, const int second) {
            if (first == 10) {
                return true;
            }
            if (first == 192 && second == 168) {
                return true;
            }
            return first == 172 && second >= 16 && second <= 31;
        }

        std::optional<std::string> ReadPrivateIpv4(const std::string &text, const std::size_t index) {
            if (index >= text.size() || !std::isdigit(static_cast<unsigned char>(text.at(index)))) {
                return std::nullopt;
            }
            if (index > 0) {
                const auto previous = static_cast<unsigned char>(text.at(index - 1));
                if (std::isdigit(previous) || text.at(index - 1) == '.') {
                    return std::nullopt;
                }
            }

            int octets[4] = {};
            std::size_t cursor = index;
            for (int part = 0; part < 4; ++part) {
                if (part > 0) {
                    if (cursor >= text.size() || text.at(cursor) != '.') {
                        return std::nullopt;
                    }
                    ++cursor;
                }
                if (cursor >= text.size() || !std::isdigit(static_cast<unsigned char>(text.at(cursor)))) {
                    return std::nullopt;
                }
                int value = 0;
                const char *begin = text.data() + cursor;
                const auto [end, error] = std::from_chars(begin, text.data() + text.size(), value);
                const auto consumed = static_cast<std::size_t>(end - begin);
                if (error != std::errc{} || consumed == 0 || consumed > 3 || value < 0 || value > 255) {
                    return std::nullopt;
                }
                octets[part] = value;
                cursor += consumed;
            }
            if (cursor < text.size()) {
                const auto next = static_cast<unsigned char>(text.at(cursor));
                if (std::isdigit(next) || text.at(cursor) == '.') {
                    return std::nullopt;
                }
            }
            if (!IsPrivateIpv4(octets[0], octets[1])) {
                return std::nullopt;
            }
            return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." + std::to_string(octets[2]) + "." +
                   std::to_string(octets[3]);
        }

        std::optional<std::string> PrivateIpv4OnLine(const std::string &line) {
            const std::string lower = Platform::LowerCopy(line);
            const auto source = lower.find(" src ");
            if (source != std::string::npos) {
                for (std::size_t index = source + 5; index < line.size(); ++index) {
                    if (std::optional<std::string> address = ReadPrivateIpv4(line, index)) {
                        return address;
                    }
                }
            }
            for (std::size_t index = 0; index < line.size(); ++index) {
                if (std::optional<std::string> address = ReadPrivateIpv4(line, index)) {
                    return address;
                }
            }
            return std::nullopt;
        }

        bool WaitForAdbRestart(const std::atomic<bool> *cancel, const int milliseconds) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
            while (std::chrono::steady_clock::now() < deadline) {
                if (cancel != nullptr && cancel->load()) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return cancel != nullptr && cancel->load();
        }
    }

    std::optional<std::string> ParseWirelessIpv4(const std::string &text) {
        const bool mentionsWlan = Platform::LowerCopy(text).find("wlan") != std::string::npos;
        const bool interfaceDump = text.find(':') != std::string::npos;
        std::optional<std::string> fallback;
        bool inWlan = false;
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string lower = Platform::LowerCopy(line);
            const bool header = !line.empty() &&
                                (std::isspace(static_cast<unsigned char>(line.front())) == 0) &&
                                lower.find(':') != std::string::npos &&
                                lower.find("inet") == std::string::npos;
            if (header) {
                inWlan = lower.find("wlan") != std::string::npos;
            } else if (lower.find("wlan") != std::string::npos) {
                inWlan = true;
            }
            if (std::optional<std::string> address = PrivateIpv4OnLine(line)) {
                if (inWlan || lower.find("wlan") != std::string::npos) {
                    return address;
                }
                if (!fallback) {
                    fallback = std::move(address);
                }
            }
        }
        if (mentionsWlan || interfaceDump) {
            return std::nullopt;
        }
        return fallback;
    }

    std::optional<std::string> FindAdbNetworkEndpoint(const std::string &text) {
        std::size_t index = 0;
        while (index < text.size()) {
            const auto current = static_cast<unsigned char>(text.at(index));
            if (std::isspace(current) || text.at(index) == '\'' || text.at(index) == '"') {
                ++index;
                continue;
            }
            std::size_t end = index;
            while (end < text.size()) {
                const auto character = static_cast<unsigned char>(text.at(end));
                if (std::isspace(character) || text.at(end) == '\'' || text.at(end) == '"') {
                    break;
                }
                ++end;
            }
            std::string token = text.substr(index, end - index);
            while (!token.empty() && (token.back() == ',' || token.back() == '.' || token.back() == ';')) {
                token.pop_back();
            }
            if (IsValidAdbNetworkEndpoint(token)) {
                return token;
            }
            index = std::max(end, index + 1);
        }
        return std::nullopt;
    }

    bool IsValidAdbSocketSpec(const std::string &endpoint) {
        if (endpoint.empty() || endpoint.find_first_of(" \t\r\n") != std::string::npos) {
            return false;
        }
        const auto separator = endpoint.find(':');
        if (separator == std::string::npos || separator + 1 >= endpoint.size()) {
            return false;
        }
        const std::string type = endpoint.substr(0, separator);
        const std::string value = endpoint.substr(separator + 1);
        if (type == "tcp") {
            int port = -1;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
            return error == std::errc{} &&
                   end == value.data() + value.size() &&
                   port >= 0 && port <= 65535;
        }
        return type == "localabstract" ||
               type == "localreserved" ||
               type == "localfilesystem" ||
               type == "dev" ||
               type == "jdwp" ||
               type == "vsock";
    }

    AdbCommandResult RunAdb(
        const SdkInfo &sdk,
        const std::vector<std::string> &command,
        const std::string &serial,
        const std::atomic<bool> *cancel,
        const std::size_t outputLimit
    ) {
        AdbCommandResult result;
        if (sdk.AdbPath.empty()) {
            result.Error = AdbError::ToolMissing;
            return result;
        }
        if (command.empty()) {
            result.Error = AdbError::InvalidRequest;
            return result;
        }

        Platform::PipedProcess process = Platform::SpawnProcessWithPipes(
            sdk.AdbPath,
            BuildAdbArguments(serial, command),
            sdk.ToolEnv
        );
        if (!process) {
            result.Error = AdbError::StartFailed;
            return result;
        }
        result.Started = true;

        std::array<char, READ_CHUNK_SIZE> stdoutBuffer{};
        std::array<char, READ_CHUNK_SIZE> stderrBuffer{};
        bool cancelled = false;
        while (process.StdoutFd >= 0 || process.StderrFd >= 0) {
            const bool readStdout = DrainPipe(process.StdoutFd, result.Stdout, stdoutBuffer, outputLimit);
            const bool readStderr = DrainPipe(process.StderrFd, result.Stderr, stderrBuffer, outputLimit);
            if (cancel != nullptr && cancel->load()) {
                cancelled = true;
                Platform::InterruptProcess(process.Pid);
                break;
            }
            if (!readStdout && !readStderr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        if (cancelled) {
            Platform::WaitForProcessResult(process, 500);
            Platform::ClosePipedProcess(process);
            result.Error = AdbError::Cancelled;
            return result;
        }

        const Platform::ProcessExitResult exit = Platform::WaitForProcessResult(process, 5000);
        Platform::ClosePipedProcess(process);
        result.ExitCode = exit.ExitCode;
        if (!exit.Exited || exit.ExitCode != 0) {
            result.Error = AdbError::CommandFailed;
        }
        return result;
    }

    AdbCommandResult RunAdbShell(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &command,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || command.empty()) {
            return {.Error = AdbError::InvalidRequest};
        }
        return RunAdb(sdk, {"shell", command}, serial, cancel);
    }

    AdbCommandResult InstallAdbPackage(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &apkPath,
        const AdbInstallOptions &options,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || apkPath.empty()) {
            return {.Error = AdbError::InvalidRequest};
        }
        std::vector<std::string> command{"install"};
        if (options.Replace) command.emplace_back("-r");
        if (options.AllowDowngrade) command.emplace_back("-d");
        if (options.GrantRuntimePermissions) command.emplace_back("-g");
        command.push_back(apkPath);
        return RunAdb(sdk, command, serial, cancel);
    }

    std::vector<std::string> UninstallAdbCommand(const std::string &packageName, const bool keepData) {
        if (keepData) {
            return {"shell", "cmd", "package", "uninstall", "-k", packageName};
        }
        return {"uninstall", packageName};
    }

    std::vector<std::string> ListAdbPackagesCommand(const AdbPackageListScope scope) {
        std::vector<std::string> command{"shell", "pm", "list", "packages"};
        switch (scope) {
            case AdbPackageListScope::ThirdParty:
                command.emplace_back("-3");
                break;
            case AdbPackageListScope::System:
                command.emplace_back("-s");
                break;
            case AdbPackageListScope::All:
                break;
        }
        return command;
    }

    std::vector<std::string> ParseAdbPackages(const std::string &output) {
        std::vector<std::string> packages;
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (std::optional<std::string> name = PackageNameFromListLine(line)) {
                packages.push_back(std::move(*name));
            }
        }
        std::ranges::sort(packages);
        const auto unique = std::ranges::unique(packages);
        packages.erase(unique.begin(), unique.end());
        return packages;
    }

    AdbCommandResult UninstallAdbPackage(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &packageName,
        const bool keepData,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || packageName.empty()) {
            return {.Error = AdbError::InvalidRequest};
        }
        return RunAdb(sdk, UninstallAdbCommand(packageName, keepData), serial, cancel);
    }

    AdbCommandResult PushAdbFile(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &localPath,
        const std::string &remotePath,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || localPath.empty() || remotePath.empty()) {
            return {.Error = AdbError::InvalidRequest};
        }
        return RunAdb(sdk, {"push", localPath, remotePath}, serial, cancel);
    }

    AdbCommandResult PullAdbFile(
        const SdkInfo &sdk,
        const std::string &serial,
        const std::string &remotePath,
        const std::string &localPath,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || remotePath.empty() || localPath.empty()) {
            return {.Error = AdbError::InvalidRequest};
        }
        return RunAdb(sdk, {"pull", remotePath, localPath}, serial, cancel);
    }

    AdbCommandResult ConnectAdbDeviceOverWifi(
        const SdkInfo &sdk,
        const std::string &serial,
        const int port,
        const std::atomic<bool> *cancel
    ) {
        if (serial.empty() || port < 1 || port > 65535) {
            return {.Error = AdbError::InvalidRequest};
        }
        if (cancel != nullptr && cancel->load()) {
            return {.Error = AdbError::Cancelled};
        }

        const std::vector<std::vector<std::string>> probes = {
            {"shell", "ip", "-f", "inet", "addr"},
            {"shell", "ip", "addr"},
            {"shell", "getprop", "dhcp.wlan0.ipaddress"},
        };
        std::string address;
        for (const std::vector<std::string> &command: probes) {
            const AdbCommandResult probe = RunAdb(sdk, command, serial, cancel);
            if (probe.Error == AdbError::Cancelled || probe.Error == AdbError::ToolMissing) {
                return probe;
            }
            if (std::optional<std::string> parsed = ParseWirelessIpv4(probe.Output())) {
                address = std::move(*parsed);
                break;
            }
        }
        if (address.empty()) {
            AdbCommandResult missing;
            missing.Started = true;
            missing.ExitCode = 1;
            missing.Error = AdbError::CommandFailed;
            return missing;
        }

        const std::string portText = std::to_string(port);
        AdbCommandResult tcpip = RunAdb(sdk, {"tcpip", portText}, serial, cancel);
        if (!tcpip.Ok()) {
            return tcpip;
        }

        const std::string endpoint = address + ":" + portText;
        AdbCommandResult connect;
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (WaitForAdbRestart(cancel, attempt == 0 ? 800 : 600)) {
                return {.Error = AdbError::Cancelled, .Started = true};
            }
            connect = RunAdb(sdk, {"connect", endpoint}, {}, cancel);
            if (connect.Error == AdbError::Cancelled || AdbConnectReportSucceeded(connect.Output())) {
                return connect;
            }
        }
        return connect;
    }
}

//
// Created by AbdulMuaz Aqeel on 14/10/2026.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "apk_signer.h"
#include "paths.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        std::string TrimCopy(std::string value) {
            while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            std::size_t start = 0;
            while (start < value.size() && (value.at(start) == ' ' || value.at(start) == '\t')) {
                ++start;
            }
            if (start > 0) {
                value.erase(0, start);
            }
            return value;
        }

        bool ApplyScheme(const std::string &line, const std::string_view prefix, bool &enabled) {
            if (!line.starts_with(prefix)) {
                return false;
            }
            const auto colon = line.rfind(':');
            if (colon == std::string::npos) {
                enabled = false;
                return true;
            }
            enabled = TrimCopy(line.substr(colon + 1)) == "true";
            return true;
        }

        void TakePrefixed(const std::string &line, const std::string_view prefix, std::string &dest) {
            if (!dest.empty() || !line.starts_with(prefix)) {
                return;
            }
            dest = TrimCopy(line.substr(prefix.size()));
        }

        bool IsRegularFile(const std::string &path) {
            if (path.empty()) {
                return false;
            }
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        }

        std::string KeytoolPath(const JdkInfo &jdk) {
            if (jdk.JavaHome.empty()) {
                return {};
            }
            return Paths::JoinPaths({jdk.JavaHome, "bin", "keytool" + Paths::GetExecutableExtension()});
        }

        bool CommandFailed(const CommandResult &result) {
            return !result.Started || result.ExitCode != 0;
        }

        void RemoveQuiet(const std::filesystem::path &path) {
            if (path.empty()) {
                return;
            }
            std::error_code error;
            std::filesystem::remove(path, error);
        }

        struct RemovedFile {
            std::filesystem::path Path;
            bool Armed = true;

            RemovedFile() = default;

            RemovedFile(const RemovedFile &) = delete;
            RemovedFile(RemovedFile &&) = delete;
            RemovedFile &operator=(const RemovedFile &) = delete;
            RemovedFile &operator=(RemovedFile &&) = delete;

            ~RemovedFile() {
                if (Armed) {
                    RemoveQuiet(Path);
                }
            }
        };

        void CloseFd(const int fd) {
            if (fd < 0) {
                return;
            }
#ifdef _WIN32
            _close(fd);
#else
            ::close(fd);
#endif
        }

        bool WriteAll(const int fd, const char *data, const std::size_t size) {
            std::size_t offset = 0;
            while (offset < size) {
                const std::size_t remaining = size - offset;
#ifdef _WIN32
                const unsigned chunk = static_cast<unsigned>(std::min<std::size_t>(remaining, 1u << 20));
                const int wrote = _write(fd, data + offset, chunk);
                if (wrote <= 0) {
                    return false;
                }
#else
                const ssize_t wrote = ::write(fd, data + offset, remaining);
                if (wrote < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                if (wrote == 0) {
                    return false;
                }
#endif
                offset += static_cast<std::size_t>(wrote);
            }
            return true;
        }

        bool SyncFd(const int fd) {
#ifdef _WIN32
            return _commit(fd) == 0;
#else
            return ::fsync(fd) == 0;
#endif
        }

        int OpenExclusive(const std::filesystem::path &path) {
            const std::string native = path.string();
#ifdef _WIN32
            return _open(native.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
            const int fd = ::open(native.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
            if (fd < 0) {
                return -1;
            }
            if (::fchmod(fd, 0600) != 0) {
                ::close(fd);
                RemoveQuiet(path);
                return -1;
            }
            return fd;
#endif
        }

        int OpenForWipe(const std::filesystem::path &path) {
            const std::string native = path.string();
#ifdef _WIN32
            return _open(native.c_str(), _O_WRONLY | _O_BINARY);
#else
            return ::open(native.c_str(), O_WRONLY | O_CLOEXEC);
#endif
        }

        bool OverwriteWithZeros(const int fd, const std::size_t size) {
#ifdef _WIN32
            if (_lseek(fd, 0, SEEK_SET) < 0) {
                return false;
            }
#else
            if (::lseek(fd, 0, SEEK_SET) < 0) {
                return false;
            }
#endif
            std::array<char, 4096> zeros{};
            std::size_t offset = 0;
            while (offset < size) {
                const std::size_t chunk = std::min(zeros.size(), size - offset);
                if (!WriteAll(fd, zeros.data(), chunk)) {
                    return false;
                }
                offset += chunk;
            }
            return true;
        }

        std::string RandomToken() {
            std::array<unsigned char, 16> bytes{};
            try {
                std::random_device device;
                std::uniform_int_distribution<unsigned int> distribution(0, 255);
                for (unsigned char &byte: bytes) {
                    byte = static_cast<unsigned char>(distribution(device));
                }
            } catch (const std::exception &) {
                const auto ticks = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#ifdef _WIN32
                const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
                const auto pid = static_cast<std::uint64_t>(::getpid());
#endif
                for (std::size_t index = 0; index < bytes.size(); ++index) {
                    const auto shift = (index % 8) * 8;
                    bytes.at(index) = static_cast<unsigned char>((ticks >> shift) ^ (pid >> shift) ^ index);
                }
            }

            std::string token(bytes.size() * 2, '0');
            constexpr char HEX[] = "0123456789abcdef";
            for (std::size_t index = 0; index < bytes.size(); ++index) {
                token.at(index * 2) = HEX[bytes.at(index) >> 4];
                token.at((index * 2) + 1) = HEX[bytes.at(index) & 0x0f];
            }
            return token;
        }

        std::filesystem::path TempDirectory() {
            std::error_code error;
            std::filesystem::path directory = std::filesystem::temp_directory_path(error);
            if (error) return {};
            return directory;
        }

        struct SecretString {
            std::string Value;

            SecretString() = default;

            explicit SecretString(std::string value) : Value(std::move(value)) {
                WipeSecret(value);
            }

            SecretString(const SecretString &) = delete;
            SecretString &operator=(const SecretString &) = delete;

            SecretString(SecretString &&other) noexcept : Value(std::move(other.Value)) {
                WipeSecret(other.Value);
            }

            SecretString &operator=(SecretString &&other) noexcept {
                if (this != &other) {
                    WipeSecret(Value);
                    Value = std::move(other.Value);
                    WipeSecret(other.Value);
                }
                return *this;
            }

            ~SecretString() {
                WipeSecret(Value);
            }
        };

        struct SecretFile {
            std::filesystem::path Path;
            std::size_t Bytes = 0;

            SecretFile() = default;

            SecretFile(const SecretFile &) = delete;
            SecretFile(SecretFile &&) = delete;
            SecretFile &operator=(const SecretFile &) = delete;
            SecretFile &operator=(SecretFile &&) = delete;

            ~SecretFile() {
                Destroy();
            }

            bool Write(const std::string &secret) {
                Destroy();
                const std::filesystem::path directory = TempDirectory();
                if (directory.empty()) {
                    return false;
                }

                int fd = -1;
                for (int attempt = 0; attempt < 8 && fd < 0; ++attempt) {
                    Path = directory / ("coredeck-ks-" + RandomToken());
                    fd = OpenExclusive(Path);
                }
                if (fd < 0) {
                    Path.clear();
                    return false;
                }

                Bytes = secret.size() + 1;
                bool ok = WriteAll(fd, secret.data(), secret.size());
                if (ok) {
                    constexpr char NEW_LINE = '\n';
                    ok = WriteAll(fd, &NEW_LINE, 1);
                }
                if (ok) {
                    ok = SyncFd(fd);
                }
                CloseFd(fd);
                if (!ok) {
                    Destroy();
                    return false;
                }
                return true;
            }

            void Destroy() {
                if (!Path.empty()) {
                    const int fd = OpenForWipe(Path);
                    if (fd >= 0) {
                        if (OverwriteWithZeros(fd, Bytes)) {
                            SyncFd(fd);
                        }
                        CloseFd(fd);
                    }
                    RemoveQuiet(Path);
                }
                Path.clear();
                Bytes = 0;
            }
        };

        bool ZipalignHelpHasPageSize(const std::string &help) {
            return help.find("-P") != std::string::npos;
        }

        std::vector<std::string> ZipalignArguments(
            const bool pageSize, const std::string &inputPath, const std::string &outputPath
        ) {
            std::vector<std::string> args;
            if (pageSize) {
                args.push_back("-P");
                args.push_back("16");
            } else {
                args.push_back("-p");
            }
            args.push_back("-f");
            args.push_back("4");
            args.push_back(inputPath);
            args.push_back(outputPath);
            return args;
        }

        std::string AlignTempPath(const std::string &outputPath) {
            const std::filesystem::path output(outputPath);
            std::filesystem::path parent = output.parent_path();
            if (parent.empty()) {
                parent = ".";
            }
            return (parent / (output.filename().string() + ".aligning-" + RandomToken())).string();
        }

        ApkSignResult Refused(const ApkSignRefusal refusal) {
            ApkSignResult result;
            result.Status = ApkSignStatus::Refused;
            result.Refusal = refusal;
            return result;
        }

        ApkSignResult CommandFailure(const ApkSignStatus status, const CommandResult &command) {
            ApkSignResult result;
            result.Status = status;
            result.Started = command.Started;
            result.ExitCode = command.ExitCode;
            result.Output = command.Output;
            return result;
        }

        std::optional<ApkSignRefusal> SignRequestRefusal(const ApkSignRequest &request) {
            if (request.ApksignerPath.empty()) {
                return ApkSignRefusal::EmptyApksigner;
            }
            if (request.Jdk.JavaHome.empty()) {
                return ApkSignRefusal::MissingJdk;
            }
            if (request.OutputPath.empty()) {
                return ApkSignRefusal::MissingOutput;
            }
            if (ApkOutputPathEqualsInput(request.InputPath, request.OutputPath)) {
                return ApkSignRefusal::OutputSameAsInput;
            }
            if (request.Align && (request.ZipalignPath.empty() || !IsRegularFile(request.ZipalignPath))) {
                return ApkSignRefusal::MissingZipalign;
            }
            return std::nullopt;
        }

        std::optional<ApkSignResult> AlignForSign(
            const ApkSignRequest &request, RemovedFile &alignTemp, std::string &inputForSign
        ) {
            if (!request.Align) {
                return std::nullopt;
            }
            alignTemp.Path = AlignTempPath(request.OutputPath);
            alignTemp.Armed = true;

            const CommandResult help = RunCommandArgsResult(request.ZipalignPath, {"-h"}, "", request.ToolEnv);
            if (!help.Started || help.ExitCode == 127) {
                return CommandFailure(ApkSignStatus::AlignFailed, help);
            }

            const CommandResult aligned = RunCommandArgsResult(
                request.ZipalignPath,
                ZipalignArguments(ZipalignHelpHasPageSize(help.Output), request.InputPath, alignTemp.Path.string()),
                "",
                request.ToolEnv
            );
            if (CommandFailed(aligned)) {
                return CommandFailure(ApkSignStatus::AlignFailed, aligned);
            }
            inputForSign = alignTemp.Path.string();
            return std::nullopt;
        }

        std::optional<ApkSignResult> PrepareKeyPassFile(
            SecretFile &keyPassFile,
            const std::string &storePassword,
            const std::string &keyPassword,
            std::string &keyPassPath
        ) {
            if (keyPassword == storePassword) {
                return std::nullopt;
            }
            if (!keyPassFile.Write(keyPassword)) {
                ApkSignResult result;
                result.Status = ApkSignStatus::SignFailed;
                return result;
            }
            keyPassPath = keyPassFile.Path.string();
            return std::nullopt;
        }

        ApkSignResult SignedApkResult(
            const ApkSignRequest &request, const CommandResult &signedApk, RemovedFile &alignTemp
        ) {
            RemoveQuiet(alignTemp.Path);
            alignTemp.Path.clear();
            alignTemp.Armed = false;

            ApkSignResult result;
            result.Status = ApkSignStatus::Signed;
            result.Started = true;
            result.ExitCode = signedApk.ExitCode;
            result.Verify = VerifyApk(request.ApksignerPath, request.OutputPath, request.ToolEnv);
            return result;
        }
    }

    ApkVerifyReport ParseApkVerifyOutput(const std::string &output) {
        ApkVerifyReport report;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(std::move(line));
            if (line.empty()) {
                continue;
            }
            if (line == "DOES NOT VERIFY") {
                report.Verified = false;
                continue;
            }
            if (line == "Verifies") {
                report.Verified = true;
                continue;
            }
            if (ApplyScheme(line, "Verified using v3.1 scheme", report.SchemeV31) ||
                ApplyScheme(line, "Verified using v1 scheme", report.SchemeV1) ||
                ApplyScheme(line, "Verified using v2 scheme", report.SchemeV2) ||
                ApplyScheme(line, "Verified using v3 scheme", report.SchemeV3) ||
                ApplyScheme(line, "Verified using v4 scheme", report.SchemeV4)) {
                continue;
            }
            TakePrefixed(line, "Signer #1 certificate DN:", report.SignerDn);
            TakePrefixed(line, "Signer #1 certificate SHA-256 digest:", report.CertificateSha256);
        }
        return report;
    }

    ApkVerifyResult VerifyApk(
        const std::string &apksignerPath,
        const std::string &apkPath,
        const EnvVars &toolEnv
    ) {
        ApkVerifyResult result;
        if (apksignerPath.empty()) {
            result.ToolMissing = true;
            return result;
        }
        if (apkPath.empty()) {
            return result;
        }

        const CommandResult command = RunCommandArgsResult(
            apksignerPath,
            {"verify", "--verbose", "--print-certs", apkPath},
            "",
            toolEnv
        );
        result.Started = command.Started;
        result.ExitCode = command.ExitCode;
        result.Output = command.Output;
        result.Report = ParseApkVerifyOutput(command.Output);
        if (CommandFailed(command)) {
            result.Report.Verified = false;
        }
        return result;
    }

    std::vector<std::string> ParseKeystoreAliases(const std::string &output) {
        std::vector<std::string> aliases;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(std::move(line));
            if (line.find("trustedCertEntry") != std::string::npos) {
                continue;
            }
            if (line.find("PrivateKeyEntry") == std::string::npos) {
                continue;
            }
            const auto comma = line.find(',');
            if (comma == std::string::npos || comma == 0) {
                continue;
            }
            std::string alias = TrimCopy(line.substr(0, comma));
            if (!alias.empty()) {
                aliases.push_back(std::move(alias));
            }
        }
        return aliases;
    }

    ApkAliasListResult ListKeystoreAliases(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const EnvVars &toolEnv
    ) {
        SecretString password{std::move(storePassword)};
        WipeSecret(storePassword);

        ApkAliasListResult result;
        const std::string keytool = KeytoolPath(jdk);
        if (jdk.JavaHome.empty() || !IsRegularFile(keytool)) {
            result.Status = ApkAliasStatus::Refused;
            result.ToolMissing = true;
            return result;
        }
        if (keystorePath.empty()) {
            result.Status = ApkAliasStatus::Refused;
            return result;
        }

        SecretFile passFile;
        if (!passFile.Write(password.Value)) {
            result.Status = ApkAliasStatus::Failed;
            return result;
        }

        const CommandResult command = RunCommandArgsResult(
            keytool,
            {"-list", "-keystore", keystorePath, "-storepass:file", passFile.Path.string()},
            "",
            toolEnv
        );
        passFile.Destroy();
        WipeSecret(password.Value);

        result.Started = command.Started;
        result.ExitCode = command.ExitCode;
        result.Output = command.Output;
        if (CommandFailed(command)) {
            result.Status = ApkAliasStatus::Failed;
            return result;
        }
        result.Status = ApkAliasStatus::Ok;
        result.Aliases = ParseKeystoreAliases(command.Output);
        return result;
    }

    std::vector<std::string> BuildApkSignArguments(
        const std::string &keystorePath,
        const std::string &keyAlias,
        const std::string &outputPath,
        const std::string &inputPath,
        const std::string &keyPassFilePath
    ) {
        std::vector<std::string> args = {
            "sign",
            "--ks",
            keystorePath,
            "--ks-key-alias",
            keyAlias,
            "--ks-pass",
            "stdin",
        };
        if (!keyPassFilePath.empty()) {
            args.push_back("--key-pass");
            args.push_back("file:" + keyPassFilePath);
        }
        args.push_back("--out");
        args.push_back(outputPath);
        args.push_back(inputPath);
        return args;
    }

    bool ApkOutputPathEqualsInput(const std::string &inputPath, const std::string &outputPath) {
        if (inputPath.empty() || outputPath.empty()) {
            return false;
        }

        const std::filesystem::path input(Paths::NormalizePath(inputPath));
        const std::filesystem::path output(Paths::NormalizePath(outputPath));
        if (input == output) {
            return true;
        }

        std::error_code inputError;
        std::error_code outputError;
        const std::filesystem::path inputCanonical = std::filesystem::weakly_canonical(input, inputError);
        const std::filesystem::path outputCanonical = std::filesystem::weakly_canonical(output, outputError);
        if (!inputError && !outputError && inputCanonical == outputCanonical) {
            return true;
        }

        if (!std::filesystem::exists(input, inputError) || !std::filesystem::exists(output, outputError)) {
            return false;
        }
        std::error_code equivalentError;
        return std::filesystem::equivalent(input, output, equivalentError);
    }

    ApkSignResult SignApk(ApkSignRequest request) {
        SecretString storePassword{std::move(request.StorePassword)};
        WipeSecret(request.StorePassword);
        SecretString keyPassword{std::move(request.KeyPassword)};
        WipeSecret(request.KeyPassword);

        if (const std::optional<ApkSignRefusal> refusal = SignRequestRefusal(request)) {
            return Refused(*refusal);
        }

        RemovedFile alignTemp;
        alignTemp.Armed = false;
        std::string inputForSign = request.InputPath;
        if (std::optional<ApkSignResult> aligned = AlignForSign(request, alignTemp, inputForSign)) {
            return std::move(*aligned);
        }

        SecretFile keyPassFile;
        std::string keyPassPath;
        if (std::optional<ApkSignResult> keyPass = PrepareKeyPassFile(
                keyPassFile,
                storePassword.Value,
                keyPassword.Value,
                keyPassPath
            )) {
            return std::move(*keyPass);
        }

        SecretString stdinPassword;
        stdinPassword.Value.reserve(storePassword.Value.size() + 1);
        stdinPassword.Value = storePassword.Value;
        stdinPassword.Value.push_back('\n');

        RemovedFile partialOutput;
        partialOutput.Path = request.OutputPath;
        partialOutput.Armed = false;

        const CommandResult signedApk = RunCommandArgsResult(
            request.ApksignerPath,
            BuildApkSignArguments(
                request.KeystorePath,
                request.KeyAlias,
                request.OutputPath,
                inputForSign,
                keyPassPath
            ),
            stdinPassword.Value,
            request.ToolEnv
        );
        WipeSecret(stdinPassword.Value);
        WipeSecret(storePassword.Value);
        WipeSecret(keyPassword.Value);
        keyPassFile.Destroy();

        if (CommandFailed(signedApk)) {
            partialOutput.Armed = signedApk.Started;
            return CommandFailure(ApkSignStatus::SignFailed, signedApk);
        }

        std::error_code outputError;
        if (!std::filesystem::is_regular_file(request.OutputPath, outputError)) {
            partialOutput.Armed = true;
            return CommandFailure(ApkSignStatus::SignFailed, signedApk);
        }
        return SignedApkResult(request, signedApk, alignTemp);
    }
}

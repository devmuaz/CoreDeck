//
// Created by AbdulMuaz Aqeel on 14/10/2026.
//

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "apk_signer.h"
#include "keytool.h"
#include "platform/paths.h"
#include "platform/process.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
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

        bool CommandFailed(const Platform::CommandResult &result) {
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

        struct SecretString {
            std::string Value;

            SecretString() = default;

            explicit SecretString(std::string value) : Value(std::move(value)) {
                Platform::WipeSecret(value);
            }

            SecretString(const SecretString &) = delete;
            SecretString &operator=(const SecretString &) = delete;

            SecretString(SecretString &&other) noexcept : Value(std::move(other.Value)) {
                Platform::WipeSecret(other.Value);
            }

            SecretString &operator=(SecretString &&other) noexcept {
                if (this != &other) {
                    Platform::WipeSecret(Value);
                    Value = std::move(other.Value);
                    Platform::WipeSecret(other.Value);
                }
                return *this;
            }

            ~SecretString() {
                Platform::WipeSecret(Value);
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

        ApkSignResult CommandFailure(const ApkSignStatus status, const Platform::CommandResult &command) {
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
            if (Platform::SameFilePath(request.InputPath, request.OutputPath)) {
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

            const Platform::CommandResult help = Platform::RunCommandArgsResult(request.ZipalignPath, {"-h"}, "", request.ToolEnv);
            if (!help.Started || help.ExitCode == 127) {
                return CommandFailure(ApkSignStatus::AlignFailed, help);
            }

            const Platform::CommandResult aligned = Platform::RunCommandArgsResult(
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
            KeystorePasswordFile &keyPassFile,
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
            keyPassPath = keyPassFile.Path().string();
            return std::nullopt;
        }

        ApkSignResult SignedApkResult(
            const ApkSignRequest &request, const Platform::CommandResult &signedApk, RemovedFile &alignTemp
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
        const Platform::EnvVars &toolEnv
    ) {
        ApkVerifyResult result;
        if (apksignerPath.empty()) {
            result.ToolMissing = true;
            return result;
        }
        if (apkPath.empty()) {
            return result;
        }

        const Platform::CommandResult command = Platform::RunCommandArgsResult(
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

    std::vector<std::string> BuildApkSignArguments(
        const std::string &keystorePath,
        const std::string &keyAlias,
        const std::string &outputPath,
        const std::string &inputPath,
        const std::string &keyPassFilePath,
        const std::string &keystoreType
    ) {
        std::vector<std::string> args = {
            "sign",
            "--ks",
            keystorePath,
        };
        if (!keystoreType.empty()) {
            args.push_back("--ks-type");
            args.push_back(keystoreType);
        }
        args.push_back("--ks-key-alias");
        args.push_back(keyAlias);
        args.push_back("--ks-pass");
        args.push_back("stdin");
        if (!keyPassFilePath.empty()) {
            args.push_back("--key-pass");
            args.push_back("file:" + keyPassFilePath);
        }
        args.push_back("--out");
        args.push_back(outputPath);
        args.push_back(inputPath);
        return args;
    }

    ApkSignResult SignApk(ApkSignRequest request) {
        SecretString storePassword{std::move(request.StorePassword)};
        Platform::WipeSecret(request.StorePassword);
        SecretString keyPassword{std::move(request.KeyPassword)};
        Platform::WipeSecret(request.KeyPassword);

        if (const std::optional<ApkSignRefusal> refusal = SignRequestRefusal(request)) {
            return Refused(*refusal);
        }

        RemovedFile alignTemp;
        alignTemp.Armed = false;
        std::string inputForSign = request.InputPath;
        if (std::optional<ApkSignResult> aligned = AlignForSign(request, alignTemp, inputForSign)) {
            return std::move(*aligned);
        }

        KeystorePasswordFile keyPassFile;
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

        const Platform::CommandResult signedApk = Platform::RunCommandArgsResult(
            request.ApksignerPath,
            BuildApkSignArguments(
                request.KeystorePath,
                request.KeyAlias,
                request.OutputPath,
                inputForSign,
                keyPassPath,
                request.KeystoreType
            ),
            stdinPassword.Value,
            request.ToolEnv
        );
        Platform::WipeSecret(stdinPassword.Value);
        Platform::WipeSecret(storePassword.Value);
        Platform::WipeSecret(keyPassword.Value);
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

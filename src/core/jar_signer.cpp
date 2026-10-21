//
// Created by AbdulMuaz Aqeel on 21/10/2026.
//

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jar_signer.h"
#include "keytool.h"
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

        bool IsRegularFile(const std::string &path) {
            if (path.empty()) {
                return false;
            }
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
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

        void AppendEnglishJvmArgs(std::vector<std::string> &args) {
            args.emplace_back("-J-Duser.language=en");
            args.emplace_back("-J-Duser.country=US");
        }

        std::string QuotedValue(const std::string &line, const std::string_view marker) {
            const auto at = line.find(marker);
            if (at == std::string::npos) {
                return {};
            }
            const auto open = line.find('"', at + marker.size());
            if (open == std::string::npos) {
                return {};
            }
            const auto close = line.find('"', open + 1);
            if (close == std::string::npos || close <= open) {
                return {};
            }
            return line.substr(open + 1, close - open - 1);
        }

        void TakePrefixed(const std::string &line, const std::string_view prefix, std::string &dest) {
            if (!dest.empty() || !line.starts_with(prefix)) {
                return;
            }
            dest = TrimCopy(line.substr(prefix.size()));
        }

        JarSignResult Refused(const JarSignRefusal refusal) {
            JarSignResult result;
            result.Status = JarSignStatus::Refused;
            result.Refusal = refusal;
            return result;
        }

        JarSignResult CommandFailure(const CommandResult &command) {
            JarSignResult result;
            result.Status = JarSignStatus::SignFailed;
            result.Started = command.Started;
            result.ExitCode = command.ExitCode;
            result.Output = command.Output;
            return result;
        }

        std::vector<std::string> BuildJarVerifyArguments(const std::string &jarPath) {
            std::vector<std::string> args;
            AppendEnglishJvmArgs(args);
            args.emplace_back("-verify");
            args.emplace_back("-verbose");
            args.emplace_back("-certs");
            args.push_back(jarPath);
            return args;
        }
    }

    std::string JarsignerPath(const JdkInfo &jdk) {
        return JdkExecutablePath(jdk, "jarsigner");
    }

    std::vector<std::string> JarArchivePatterns() {
        return {
            "*.jar",
            "*.JAR",
            "*.war",
            "*.WAR",
            "*.ear",
            "*.EAR",
            "*.aab",
            "*.AAB",
        };
    }

    std::string SuggestedSignedJarPath(const std::string &inputPath) {
        const std::filesystem::path input(inputPath);
        std::string stem = input.stem().string();
        if (stem.empty()) {
            stem = input.filename().string();
        }
        if (stem.empty()) {
            stem = "signed";
        }
        return (input.parent_path() / (stem + "-signed" + input.extension().string())).string();
    }

    JarVerifyReport ParseJarVerifyOutput(const std::string &output) {
        JarVerifyReport report;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(std::move(line));
            if (line.empty()) {
                continue;
            }
            if (line.starts_with("jar verified.")) {
                report.Verified = true;
                continue;
            }
            if (line.starts_with("jar is unsigned")) {
                report.Verified = false;
                continue;
            }
            if (report.Signer.empty()) {
                report.Signer = QuotedValue(line, "- Signed by");
            }
            if (report.Timestamp.empty()) {
                report.Timestamp = QuotedValue(line, "Timestamped by");
            }
            TakePrefixed(line, "Digest algorithm:", report.DigestAlgorithm);
            TakePrefixed(line, "Signature algorithm:", report.SignatureAlgorithm);
        }
        return report;
    }

    JarVerifyResult VerifyJar(
        const std::string &jarsignerPath,
        const std::string &jarPath,
        const EnvVars &toolEnv
    ) {
        JarVerifyResult result;
        if (jarsignerPath.empty()) {
            result.ToolMissing = true;
            return result;
        }
        if (jarPath.empty()) {
            return result;
        }

        const CommandResult command = RunCommandArgsResult(
            jarsignerPath,
            BuildJarVerifyArguments(jarPath),
            "",
            toolEnv
        );
        result.Started = command.Started;
        result.ExitCode = command.ExitCode;
        result.Output = command.Output;
        result.Report = ParseJarVerifyOutput(command.Output);
        if (CommandFailed(command)) {
            result.Report.Verified = false;
        }
        return result;
    }

    std::vector<std::string> BuildJarSignArguments(
        const std::string &keystorePath,
        const std::string &keyAlias,
        const std::string &outputPath,
        const std::string &inputPath,
        const std::string &storePassFile,
        const std::string &keyPassFile,
        const std::string &keystoreType
    ) {
        std::vector<std::string> args;
        AppendEnglishJvmArgs(args);
        args.emplace_back("-keystore");
        args.push_back(keystorePath);
        if (!keystoreType.empty()) {
            args.emplace_back("-storetype");
            args.push_back(keystoreType);
        }
        args.emplace_back("-storepass:file");
        args.push_back(storePassFile);
        if (!keyPassFile.empty()) {
            args.emplace_back("-keypass:file");
            args.push_back(keyPassFile);
        }
        args.emplace_back("-signedjar");
        args.push_back(outputPath);
        args.push_back(inputPath);
        args.push_back(keyAlias);
        return args;
    }

    JarSignResult SignJar(JarSignRequest request) {
        SecretString storePassword{std::move(request.StorePassword)};
        WipeSecret(request.StorePassword);
        SecretString keyPassword{std::move(request.KeyPassword)};
        WipeSecret(request.KeyPassword);

        if (request.Jdk.JavaHome.empty()) {
            return Refused(JarSignRefusal::MissingJdk);
        }
        const std::string jarsigner = JarsignerPath(request.Jdk);
        if (jarsigner.empty() || !IsRegularFile(jarsigner)) {
            return Refused(JarSignRefusal::MissingJarsigner);
        }
        if (request.OutputPath.empty()) {
            return Refused(JarSignRefusal::MissingOutput);
        }
        if (Paths::SameFilePath(request.InputPath, request.OutputPath)) {
            return Refused(JarSignRefusal::OutputSameAsInput);
        }

        KeystorePasswordFile storePassFile;
        if (!storePassFile.Write(storePassword.Value)) {
            return Refused(JarSignRefusal::PasswordFileFailed);
        }
        KeystorePasswordFile keyPassFile;
        std::string keyPassPath;
        if (keyPassword.Value != storePassword.Value) {
            if (!keyPassFile.Write(keyPassword.Value)) {
                return Refused(JarSignRefusal::PasswordFileFailed);
            }
            keyPassPath = keyPassFile.Path().string();
        }

        RemovedFile partialOutput;
        partialOutput.Path = request.OutputPath;
        partialOutput.Armed = false;

        const CommandResult signedJar = RunCommandArgsResult(
            jarsigner,
            BuildJarSignArguments(
                request.KeystorePath,
                request.KeyAlias,
                request.OutputPath,
                request.InputPath,
                storePassFile.Path().string(),
                keyPassPath,
                request.KeystoreType
            ),
            "",
            request.ToolEnv
        );
        storePassFile.Destroy();
        keyPassFile.Destroy();
        WipeSecret(storePassword.Value);
        WipeSecret(keyPassword.Value);

        if (CommandFailed(signedJar)) {
            partialOutput.Armed = signedJar.Started;
            return CommandFailure(signedJar);
        }

        std::error_code outputError;
        if (!std::filesystem::is_regular_file(request.OutputPath, outputError)) {
            partialOutput.Armed = true;
            return CommandFailure(signedJar);
        }

        partialOutput.Armed = false;
        JarSignResult result;
        result.Status = JarSignStatus::Signed;
        result.Started = true;
        result.ExitCode = signedJar.ExitCode;
        result.Output = signedJar.Output;
        result.Verify = VerifyJar(jarsigner, request.OutputPath, request.ToolEnv);
        return result;
    }
}

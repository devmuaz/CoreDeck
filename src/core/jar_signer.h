//
// Created by AbdulMuaz Aqeel on 21/10/2026.
//

#ifndef COREDECK_JAR_SIGNER_H
#define COREDECK_JAR_SIGNER_H

#include <cstdint>
#include <string>
#include <vector>

#include "jdk.h"

namespace CoreDeck::Core {
    struct JarVerifyReport {
        bool Verified = false;
        std::string Signer;
        std::string DigestAlgorithm;
        std::string SignatureAlgorithm;
        std::string Timestamp;
    };

    struct JarVerifyResult {
        bool ToolMissing = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        JarVerifyReport Report;
    };

    enum class JarSignStatus : uint8_t {
        Signed,
        Refused,
        SignFailed,
    };

    enum class JarSignRefusal : uint8_t {
        None,
        MissingJarsigner,
        MissingJdk,
        MissingOutput,
        OutputSameAsInput,
        PasswordFileFailed,
    };

    struct JarSignRequest {
        std::string InputPath;
        std::string OutputPath;
        std::string KeystorePath;
        std::string KeystoreType;
        std::string KeyAlias;
        std::string StorePassword;
        std::string KeyPassword;
        JdkInfo Jdk;
        EnvVars ToolEnv;
    };

    struct JarSignResult {
        JarSignStatus Status = JarSignStatus::Refused;
        JarSignRefusal Refusal = JarSignRefusal::None;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        JarVerifyResult Verify;
    };

    std::string JarsignerPath(const JdkInfo &jdk);

    std::vector<std::string> JarArchivePatterns();

    std::string SuggestedSignedJarPath(const std::string &inputPath);

    JarVerifyReport ParseJarVerifyOutput(const std::string &output);

    JarVerifyResult VerifyJar(
        const std::string &jarsignerPath,
        const std::string &jarPath,
        const EnvVars &toolEnv
    );

    std::vector<std::string> BuildJarSignArguments(
        const std::string &keystorePath,
        const std::string &keyAlias,
        const std::string &outputPath,
        const std::string &inputPath,
        const std::string &storePassFile,
        const std::string &keyPassFile = {},
        const std::string &keystoreType = {}
    );

    JarSignResult SignJar(JarSignRequest request);
}

#endif // COREDECK_JAR_SIGNER_H

//
// Created by AbdulMuaz Aqeel on 14/10/2026.
//

#ifndef COREDECK_APK_SIGNER_H
#define COREDECK_APK_SIGNER_H

#include <cstdint>
#include <string>
#include <vector>

#include "jdk.h"

namespace CoreDeck::Core {
    struct ApkVerifyReport {
        bool Verified = false;
        bool SchemeV1 = false;
        bool SchemeV2 = false;
        bool SchemeV3 = false;
        bool SchemeV31 = false;
        bool SchemeV4 = false;
        std::string SignerDn;
        std::string CertificateSha256;
    };

    struct ApkVerifyResult {
        bool ToolMissing = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        ApkVerifyReport Report;
    };

    enum class ApkSignStatus : uint8_t {
        Signed,
        Refused,
        AlignFailed,
        SignFailed,
    };

    enum class ApkSignRefusal : uint8_t {
        None,
        EmptyApksigner,
        MissingJdk,
        MissingZipalign,
        MissingOutput,
        OutputSameAsInput,
    };

    struct ApkSignRequest {
        std::string ApksignerPath;
        std::string ZipalignPath;
        std::string InputPath;
        std::string OutputPath;
        std::string KeystorePath;
        std::string KeystoreType;
        std::string KeyAlias;
        std::string StorePassword;
        std::string KeyPassword;
        bool Align = true;
        JdkInfo Jdk;
        Platform::EnvVars ToolEnv;
    };

    struct ApkSignResult {
        ApkSignStatus Status = ApkSignStatus::Refused;
        ApkSignRefusal Refusal = ApkSignRefusal::None;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        ApkVerifyResult Verify;
    };

    ApkVerifyReport ParseApkVerifyOutput(const std::string &output);

    ApkVerifyResult VerifyApk(
        const std::string &apksignerPath,
        const std::string &apkPath,
        const Platform::EnvVars &toolEnv
    );

    std::vector<std::string> BuildApkSignArguments(
        const std::string &keystorePath,
        const std::string &keyAlias,
        const std::string &outputPath,
        const std::string &inputPath,
        const std::string &keyPassFilePath = {},
        const std::string &keystoreType = {}
    );

    ApkSignResult SignApk(ApkSignRequest request);
}

#endif // COREDECK_APK_SIGNER_H

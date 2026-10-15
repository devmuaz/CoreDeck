#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "core/apk_signer.h"

using namespace CoreDeck;

TEST_CASE("ParseApkVerifyOutput reads scheme lines, signer DN, and SHA-256", "[apk-signer]") {
    const auto report = ParseApkVerifyOutput(
        "Verifies\n"
        "Verified using v1 scheme (JAR signing): true\n"
        "Verified using v2 scheme (APK Signature Scheme v2): false\n"
        "Verified using v3 scheme (APK Signature Scheme v3): true\n"
        "Verified using v3.1 scheme (APK Signature Scheme v3.1): false\n"
        "Verified using v4 scheme (APK Signature Scheme v4): true\n"
        "Verified for SourceStamp: false\n"
        "Number of signers: 1\n"
        "Signer #1 certificate DN: CN=Android Debug, O=Android, C=US\n"
        "Signer #1 certificate SHA-256 digest: a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc\n"
        "Signer #1 certificate SHA-1 digest: 5e8f16062ea3cd2c4a0d547876baa6f38cabf625\n"
        "Signer #1 key algorithm: RSA\n"
    );

    REQUIRE(report.Verified);
    REQUIRE(report.SchemeV1);
    REQUIRE_FALSE(report.SchemeV2);
    REQUIRE(report.SchemeV3);
    REQUIRE_FALSE(report.SchemeV31);
    REQUIRE(report.SchemeV4);
    REQUIRE(report.SignerDn == "CN=Android Debug, O=Android, C=US");
    REQUIRE(report.CertificateSha256 == "a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc");
}

TEST_CASE("ParseApkVerifyOutput does not treat v3.1 as v3", "[apk-signer]") {
    const auto report = ParseApkVerifyOutput(
        "Verifies\n"
        "Verified using v3.1 scheme (APK Signature Scheme v3.1): true\n"
        "Signer #1 certificate DN: CN=Rotated, O=CoreDeck, C=US\n"
    );

    REQUIRE(report.Verified);
    REQUIRE(report.SchemeV31);
    REQUIRE_FALSE(report.SchemeV1);
    REQUIRE_FALSE(report.SchemeV2);
    REQUIRE_FALSE(report.SchemeV3);
    REQUIRE_FALSE(report.SchemeV4);
    REQUIRE(report.SignerDn == "CN=Rotated, O=CoreDeck, C=US");
}

TEST_CASE("ParseApkVerifyOutput leaves Verified false on DOES NOT VERIFY", "[apk-signer]") {
    const auto report = ParseApkVerifyOutput(
        "DOES NOT VERIFY\n"
        "ERROR: JAR signer CERT.RSA: APK Signature Scheme v2 signature was not found\n"
        "Verified using v1 scheme (JAR signing): false\n"
        "Verified using v2 scheme (APK Signature Scheme v2): false\n"
        "Verified using v3 scheme (APK Signature Scheme v3): false\n"
        "Verified using v3.1 scheme (APK Signature Scheme v3.1): false\n"
        "Verified using v4 scheme (APK Signature Scheme v4): false\n"
    );

    REQUIRE_FALSE(report.Verified);
    REQUIRE_FALSE(report.SchemeV1);
    REQUIRE_FALSE(report.SchemeV2);
    REQUIRE_FALSE(report.SchemeV3);
    REQUIRE_FALSE(report.SchemeV31);
    REQUIRE_FALSE(report.SchemeV4);
    REQUIRE(report.SignerDn.empty());
    REQUIRE(report.CertificateSha256.empty());
}

TEST_CASE("ApkOutputPathEqualsInput treats a normalized path as the same file", "[apk-signer]") {
    const std::string input = "/tmp/coredeck-apk-sign/app.apk";

    REQUIRE(ApkOutputPathEqualsInput(input, input));
    REQUIRE(ApkOutputPathEqualsInput(input, "/tmp/coredeck-apk-sign/./app.apk"));
    REQUIRE(ApkOutputPathEqualsInput(input, "/tmp/coredeck-apk-sign/nested/../app.apk"));
    REQUIRE_FALSE(ApkOutputPathEqualsInput(input, "/tmp/coredeck-apk-sign/app-signed.apk"));
}

TEST_CASE("BuildApkSignArguments keeps the store password off argv", "[apk-signer]") {
    const std::vector<std::string> args = BuildApkSignArguments(
        "/tmp/release.keystore",
        "upload",
        "/tmp/app-signed.apk",
        "/tmp/app.apk"
    );

    REQUIRE(args == std::vector<std::string>{
                        "sign",
                        "--ks",
                        "/tmp/release.keystore",
                        "--ks-key-alias",
                        "upload",
                        "--ks-pass",
                        "stdin",
                        "--out",
                        "/tmp/app-signed.apk",
                        "/tmp/app.apk",
                    });

    // The builder takes no password. This sentinel is documentation only.
    for (const std::string &arg: args) {
        REQUIRE(arg.find("pass:") == std::string::npos);
        REQUIRE(arg.find(':') == std::string::npos);
        REQUIRE(arg.find("super-secret-password") == std::string::npos);
    }
    REQUIRE(std::find(args.begin(), args.end(), "--key-pass") == args.end());
}

TEST_CASE("BuildApkSignArguments adds a key password file without the password", "[apk-signer]") {
    const std::string keyPassFile = "/tmp/coredeck-key-pass";
    const std::vector<std::string> args = BuildApkSignArguments(
        "/tmp/release.keystore",
        "upload",
        "/tmp/app-signed.apk",
        "/tmp/app.apk",
        keyPassFile
    );

    REQUIRE(args == std::vector<std::string>{
                        "sign",
                        "--ks",
                        "/tmp/release.keystore",
                        "--ks-key-alias",
                        "upload",
                        "--ks-pass",
                        "stdin",
                        "--key-pass",
                        "file:" + keyPassFile,
                        "--out",
                        "/tmp/app-signed.apk",
                        "/tmp/app.apk",
                    });

    for (const std::string &arg: args) {
        REQUIRE(arg.find("pass:") == std::string::npos);
        REQUIRE(arg.find("super-secret-password") == std::string::npos);
    }
}

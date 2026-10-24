#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "core/jar_signer.h"

using namespace CoreDeck::Core;
using namespace CoreDeck;

TEST_CASE("ParseJarVerifyOutput reads the signer, algorithms, and timestamp", "[jar-signer]") {
    const auto report = ParseJarVerifyOutput(
        "- Signed by \"CN=Android Debug, O=Android, C=US\"\n"
        "    Digest algorithm: SHA-256\n"
        "    Signature algorithm: SHA256withRSA, 2048-bit RSA key\n"
        "    Timestamped by \"CN=DigiCert Timestamp, O=DigiCert, C=US\" on Wed Oct 21 09:00:00 UTC 2026\n"
        "    Timestamp digest algorithm: SHA-384\n"
        "    Timestamp signature algorithm: SHA384withRSA, 3072-bit RSA key\n"
        "\n"
        "jar verified.\n"
    );

    REQUIRE(report.Verified);
    REQUIRE(report.Signer == "CN=Android Debug, O=Android, C=US");
    REQUIRE(report.DigestAlgorithm == "SHA-256");
    REQUIRE(report.SignatureAlgorithm == "SHA256withRSA, 2048-bit RSA key");
    REQUIRE(report.Timestamp == "CN=DigiCert Timestamp, O=DigiCert, C=US");
}

TEST_CASE("ParseJarVerifyOutput keeps the first signer", "[jar-signer]") {
    const auto report = ParseJarVerifyOutput(
        "- Signed by \"CN=First, O=CoreDeck, C=US\"\n"
        "    Digest algorithm: SHA-256\n"
        "    Signature algorithm: SHA256withRSA, 2048-bit RSA key\n"
        "- Signed by \"CN=Second, O=CoreDeck, C=US\"\n"
        "    Digest algorithm: SHA-512\n"
        "    Signature algorithm: SHA512withRSA, 4096-bit RSA key\n"
        "jar verified.\n"
    );

    REQUIRE(report.Verified);
    REQUIRE(report.Signer == "CN=First, O=CoreDeck, C=US");
    REQUIRE(report.DigestAlgorithm == "SHA-256");
    REQUIRE(report.SignatureAlgorithm == "SHA256withRSA, 2048-bit RSA key");
}

TEST_CASE("ParseJarVerifyOutput leaves Verified false when the jar is unsigned", "[jar-signer]") {
    const auto report = ParseJarVerifyOutput("jar is unsigned.\n");

    REQUIRE_FALSE(report.Verified);
    REQUIRE(report.Signer.empty());
    REQUIRE(report.DigestAlgorithm.empty());
    REQUIRE(report.SignatureAlgorithm.empty());
    REQUIRE(report.Timestamp.empty());
}

TEST_CASE("SuggestedSignedJarPath keeps the archive extension", "[jar-signer]") {
    REQUIRE(SuggestedSignedJarPath("/tmp/app.aab") == "/tmp/app-signed.aab");
    REQUIRE(SuggestedSignedJarPath("/tmp/lib.jar") == "/tmp/lib-signed.jar");
    REQUIRE(SuggestedSignedJarPath("/tmp/service.war") == "/tmp/service-signed.war");
    REQUIRE(SuggestedSignedJarPath("/tmp/bundle") == "/tmp/bundle-signed");
}

TEST_CASE("JarArchivePatterns lists jar archives and app bundles", "[jar-signer]") {
    const auto patterns = JarArchivePatterns();

    REQUIRE(std::find(patterns.begin(), patterns.end(), "*.jar") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "*.war") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "*.ear") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "*.aab") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "*.apk") == patterns.end());
}

TEST_CASE("BuildJarSignArguments keeps passwords off argv", "[jar-signer]") {
    const std::string storePassFile = "/tmp/coredeck-store-pass";
    const std::vector<std::string> args = BuildJarSignArguments(
        "/tmp/release.p12",
        "upload",
        "/tmp/app-signed.aab",
        "/tmp/app.aab",
        storePassFile
    );

    REQUIRE(args == std::vector<std::string>{
                        "-J-Duser.language=en",
                        "-J-Duser.country=US",
                        "-keystore",
                        "/tmp/release.p12",
                        "-storepass:file",
                        storePassFile,
                        "-signedjar",
                        "/tmp/app-signed.aab",
                        "/tmp/app.aab",
                        "upload",
                    });

    for (const std::string &arg: args) {
        REQUIRE(arg.find("super-secret-password") == std::string::npos);
    }
    REQUIRE(std::find(args.begin(), args.end(), "-keypass:file") == args.end());
}

TEST_CASE("BuildJarSignArguments adds a key password file and store type", "[jar-signer]") {
    const std::string storePassFile = "/tmp/coredeck-store-pass";
    const std::string keyPassFile = "/tmp/coredeck-key-pass";
    const std::vector<std::string> args = BuildJarSignArguments(
        "/tmp/release.jceks",
        "upload",
        "/tmp/app-signed.jar",
        "/tmp/app.jar",
        storePassFile,
        keyPassFile,
        "JCEKS"
    );

    REQUIRE(args == std::vector<std::string>{
                        "-J-Duser.language=en",
                        "-J-Duser.country=US",
                        "-keystore",
                        "/tmp/release.jceks",
                        "-storetype",
                        "JCEKS",
                        "-storepass:file",
                        storePassFile,
                        "-keypass:file",
                        keyPassFile,
                        "-signedjar",
                        "/tmp/app-signed.jar",
                        "/tmp/app.jar",
                        "upload",
                    });

    for (const std::string &arg: args) {
        REQUIRE(arg.find("super-secret-password") == std::string::npos);
    }
}

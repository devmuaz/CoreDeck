#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/keytool.h"
#include "platform/utilities.h"

using namespace CoreDeck::Core;
using namespace CoreDeck;

namespace {
    CreateKeystoreRequest ValidCreateRequest(const std::filesystem::path &path) {
        CreateKeystoreRequest request;
        request.KeystorePath = path.string();
        request.Alias = "release";
        request.StorePassword = "secret1";
        request.CommonName = "Ada Lovelace";
        request.ValidityDays = 3650;
        request.KeySize = 2048;
        return request;
    }
}

TEST_CASE("ParseKeystoreAliases keeps PrivateKeyEntry aliases", "[keytool]") {
    const auto aliases = ParseKeystoreAliases(
        "Keystore type: PKCS12\n"
        "Keystore provider: SUN\n"
        "\n"
        "Your keystore contains 3 entries\n"
        "\n"
        "myalias, 1 Jan 2024, PrivateKeyEntry,\n"
        "Certificate fingerprint (SHA-256): C0:F4:9F:70\n"
        "my-release, 14 Oct 2026, PrivateKeyEntry, \n"
        "Certificate fingerprint (SHA-256): AA:BB:CC:DD\n"
        "platform, 1 Jan 2024, trustedCertEntry,\n"
        "Certificate fingerprint (SHA-256): 11:22:33:44\n"
    );

    REQUIRE(aliases.size() == 2);
    REQUIRE(aliases.at(0) == "myalias");
    REQUIRE(aliases.at(1) == "my-release");
}

TEST_CASE("ParseKeystoreDescription keeps every entry type", "[keytool]") {
    const KeystoreDescription description = ParseKeystoreDescription(
        "Keystore type: JCEKS\n"
        "Keystore provider: SunJCE\n"
        "\n"
        "Your keystore contains 3 entries\n"
        "\n"
        "Alias name: release\n"
        "Creation date: 16 Oct 2026\n"
        "Entry type: PrivateKeyEntry\n"
        "Certificate chain length: 2\n"
        "Certificate[1]:\n"
        "Owner: CN=Ada Lovelace, O=CoreDeck, C=GB\n"
        "Issuer: CN=Intermediate, O=CoreDeck, C=US\n"
        "Serial number: 4a2f1c\n"
        "Valid from: Fri Oct 16 01:00:00 UTC 2026 until: Mon Oct 14 01:00:00 UTC 2036\n"
        "Certificate fingerprints:\n"
        "\t SHA-1: AA:BB:CC:DD\n"
        "\t SHA-256: 11:22:33:44\n"
        "Signature algorithm name: SHA384withRSA\n"
        "Subject Public Key Algorithm: 4096-bit RSA key\n"
        "Version: 3\n"
        "Certificate[2]:\n"
        "Owner: CN=Intermediate, O=CoreDeck, C=US\n"
        "Issuer: CN=Root, O=CoreDeck, C=US\n"
        "Serial number: ff01\n"
        "Valid from: Thu Jan 01 00:00:00 UTC 2026 until: Tue Jan 01 00:00:00 UTC 2040\n"
        "Certificate fingerprints:\n"
        "\t SHA-256: 77:88\n"
        "Signature algorithm name: SHA256withRSA\n"
        "Subject Public Key Algorithm: 2048-bit RSA key\n"
        "Version: 3\n"
        "\n"
        "*******************************************\n"
        "*******************************************\n"
        "\n"
        "Alias name: platform\n"
        "Creation date: 1 Jan 2024\n"
        "Entry type: trustedCertEntry\n"
        "\n"
        "Owner: CN=Android, O=Google, C=US\n"
        "Issuer: CN=Android, O=Google, C=US\n"
        "Serial number: 9b\n"
        "Valid from: Wed Jan 01 00:00:00 UTC 2014 until: Fri Jan 01 00:00:00 UTC 2044\n"
        "Certificate fingerprints:\n"
        "\t SHA-256: EF:01\n"
        "Signature algorithm name: SHA256withRSA\n"
        "Subject Public Key Algorithm: 2048-bit RSA key\n"
        "Version: 3\n"
        "\n"
        "*******************************************\n"
        "*******************************************\n"
        "\n"
        "Alias name: aes-key\n"
        "Creation date: 2 Feb 2025\n"
        "Entry type: SecretKeyEntry\n"
    );

    REQUIRE(description.StoreType == "JCEKS");
    REQUIRE(description.Provider == "SunJCE");
    REQUIRE(description.Entries.size() == 3);

    const KeystoreEntry &release = description.Entries.at(0);
    REQUIRE(release.Alias == "release");
    REQUIRE(release.CreationDate == "16 Oct 2026");
    REQUIRE(release.EntryType == "PrivateKeyEntry");
    REQUIRE(release.Certificates.size() == 2);
    REQUIRE(release.Certificates.at(0).Owner == "CN=Ada Lovelace, O=CoreDeck, C=GB");
    REQUIRE(release.Certificates.at(0).Issuer == "CN=Intermediate, O=CoreDeck, C=US");
    REQUIRE(release.Certificates.at(0).SerialNumber == "4a2f1c");
    REQUIRE(release.Certificates.at(0).ValidFrom == "Fri Oct 16 01:00:00 UTC 2026");
    REQUIRE(release.Certificates.at(0).ValidUntil == "Mon Oct 14 01:00:00 UTC 2036");
    REQUIRE(release.Certificates.at(0).SignatureAlgorithm == "SHA384withRSA");
    REQUIRE(release.Certificates.at(0).PublicKey == "4096-bit RSA key");
    REQUIRE(release.Certificates.at(0).Fingerprints.size() == 2);
    REQUIRE(release.Certificates.at(0).Fingerprints.at(0).Algorithm == "SHA-1");
    REQUIRE(release.Certificates.at(0).Fingerprints.at(0).Value == "AA:BB:CC:DD");
    REQUIRE(release.Certificates.at(0).Fingerprints.at(1).Algorithm == "SHA-256");
    REQUIRE(release.Certificates.at(1).Owner == "CN=Intermediate, O=CoreDeck, C=US");
    REQUIRE(release.Certificates.at(1).Fingerprints.at(0).Value == "77:88");

    REQUIRE(description.Entries.at(1).Alias == "platform");
    REQUIRE(description.Entries.at(1).EntryType == "trustedCertEntry");
    REQUIRE(description.Entries.at(1).Certificates.size() == 1);
    REQUIRE(description.Entries.at(1).Certificates.at(0).Owner == "CN=Android, O=Google, C=US");

    REQUIRE(description.Entries.at(2).Alias == "aes-key");
    REQUIRE(description.Entries.at(2).EntryType == "SecretKeyEntry");
    REQUIRE(description.Entries.at(2).Certificates.empty());
}

TEST_CASE("KeystoreTypeFromMagic recognizes the file keystore formats", "[keytool]") {
    const std::uint8_t jks[] = {0xFE, 0xED, 0xFE, 0xED, 0x00};
    const std::uint8_t jceks[] = {0xCE, 0xCE, 0xCE, 0xCE};
    const std::uint8_t pkcs12[] = {0x30, 0x82, 0x01};
    const std::uint8_t unknown[] = {0x00, 0x01, 0x02, 0x03};

    REQUIRE(KeystoreTypeFromMagic(jks, sizeof jks) == "JKS");
    REQUIRE(KeystoreTypeFromMagic(jceks, sizeof jceks) == "JCEKS");
    REQUIRE(KeystoreTypeFromMagic(pkcs12, sizeof pkcs12) == "PKCS12");
    REQUIRE(KeystoreTypeFromMagic(unknown, sizeof unknown).empty());
    REQUIRE(KeystoreTypeFromMagic(jks, 2).empty());
}

TEST_CASE("KeystorePasswordRejected matches keytool's English error", "[keytool]") {
    REQUIRE(KeystorePasswordRejected("keytool error: java.io.IOException: keystore password was incorrect\n"));
    REQUIRE(KeystorePasswordRejected("Keystore was tampered with, or password was incorrect"));
    REQUIRE_FALSE(KeystorePasswordRejected("keytool error: java.io.IOException: Invalid keystore format"));
}

TEST_CASE("BuildDistinguishedName escapes reserved characters and uppercases the country", "[keytool]") {
    CreateKeystoreRequest request;
    request.CommonName = "Ada, Lovelace";
    request.Organization = "Core\\Deck";
    request.OrganizationalUnit = "  ";
    request.Locality = "London";
    request.Country = "gb";

    REQUIRE(BuildDistinguishedName(request) == "CN=Ada\\, Lovelace, O=Core\\\\Deck, L=London, C=GB");
}

TEST_CASE("ValidateCreateKeystore rejects incomplete keystores", "[keytool]") {
    const auto directory = std::filesystem::temp_directory_path();
    const auto missing = directory / "coredeck-keytool-missing-parent" / "release.p12";
    CreateKeystoreRequest request = ValidCreateRequest(missing);
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::ParentMissing);

    const auto existing = directory / "coredeck-keytool-existing.p12";
    {
        std::ofstream output(existing);
        output << "x";
    }
    request = ValidCreateRequest(existing);
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::PathExists);
    std::filesystem::remove(existing);

    const auto path = directory / "coredeck-keytool-new.p12";
    std::filesystem::remove(path);
    request = ValidCreateRequest(path);
    request.Alias = "  ";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::EmptyAlias);

    request = ValidCreateRequest(path);
    request.StorePassword = "short";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::PasswordTooShort);

    request = ValidCreateRequest(path);
    request.KeyPassword = "other-secret";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::KeyPasswordDiffers);

    request = ValidCreateRequest(path);
    request.StoreType = KeystoreType::Jks;
    request.KeyPassword = "tiny";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::KeyPasswordTooShort);

    request = ValidCreateRequest(path);
    request.ValidityDays = 0;
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::InvalidValidity);

    request = ValidCreateRequest(path);
    request.Algorithm = KeyAlgorithm::Ec;
    request.KeySize = 2048;
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::InvalidKeySize);

    request = ValidCreateRequest(path);
    request.Country = "USA";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::InvalidCountry);

    request = ValidCreateRequest(path);
    request.CommonName.clear();
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::EmptyDistinguishedName);

    request = ValidCreateRequest(path);
    request.Algorithm = KeyAlgorithm::Ed25519;
    request.KeySize = 0;
    request.StoreType = KeystoreType::Jceks;
    request.KeyPassword = "secret1";
    REQUIRE(ValidateCreateKeystore(request) == KeytoolRefusal::None);
    Platform::WipeSecret(request.StorePassword);
    Platform::WipeSecret(request.KeyPassword);
}

TEST_CASE("BuildCreateKeystoreArguments keeps passwords off the command line", "[keytool]") {
    CreateKeystoreRequest request;
    request.KeystorePath = "/tmp/release.p12";
    request.StoreType = KeystoreType::Pkcs12;
    request.Alias = " upload ";
    request.Algorithm = KeyAlgorithm::Rsa;
    request.KeySize = 4096;
    request.ValidityDays = 3650;
    request.CommonName = "Ada, Lovelace";
    request.Organization = "Core\\Deck";
    request.Country = "gb";
    request.StorePassword = "super-secret-password";
    request.KeyPassword = "other-secret";

    const std::vector<std::string> args = BuildCreateKeystoreArguments(request, "/tmp/coredeck-store-pass", "");
    REQUIRE(args == std::vector<std::string>{
                        "-J-Duser.language=en",
                        "-J-Duser.country=US",
                        "-genkeypair",
                        "-keystore",
                        "/tmp/release.p12",
                        "-storetype",
                        "PKCS12",
                        "-alias",
                        "upload",
                        "-keyalg",
                        "RSA",
                        "-keysize",
                        "4096",
                        "-validity",
                        "3650",
                        "-dname",
                        "CN=Ada\\, Lovelace, O=Core\\\\Deck, C=GB",
                        "-storepass:file",
                        "/tmp/coredeck-store-pass",
                    });
    for (const std::string &arg: args) {
        REQUIRE(arg.find("super-secret-password") == std::string::npos);
        REQUIRE(arg.find("other-secret") == std::string::npos);
    }
    Platform::WipeSecret(request.StorePassword);
    Platform::WipeSecret(request.KeyPassword);
}

TEST_CASE("BuildCreateKeystoreArguments follows the selected key", "[keytool]") {
    CreateKeystoreRequest request;
    request.KeystorePath = "/tmp/release.jks";
    request.StoreType = KeystoreType::Jks;
    request.Alias = "signing";
    request.Algorithm = KeyAlgorithm::Ec;
    request.KeySize = 256;
    request.ValidityDays = 825;
    request.CommonName = "Example";
    request.StorePassword = "secret1";
    request.KeyPassword = "secret2";

    const std::vector<std::string> ec = BuildCreateKeystoreArguments(request, "/tmp/store-pass", "/tmp/key-pass");
    REQUIRE(ec == std::vector<std::string>{
                      "-J-Duser.language=en",
                      "-J-Duser.country=US",
                      "-genkeypair",
                      "-keystore",
                      "/tmp/release.jks",
                      "-storetype",
                      "JKS",
                      "-alias",
                      "signing",
                      "-keyalg",
                      "EC",
                      "-keysize",
                      "256",
                      "-validity",
                      "825",
                      "-dname",
                      "CN=Example",
                      "-storepass:file",
                      "/tmp/store-pass",
                      "-keypass:file",
                      "/tmp/key-pass",
                  });

    request.KeystorePath = "/tmp/release.jceks";
    request.StoreType = KeystoreType::Jceks;
    request.Algorithm = KeyAlgorithm::Ed448;
    request.KeySize = 2048;
    const std::vector<std::string> ed = BuildCreateKeystoreArguments(request, "/tmp/store-pass", "/tmp/key-pass");
    REQUIRE(ed == std::vector<std::string>{
                      "-J-Duser.language=en",
                      "-J-Duser.country=US",
                      "-genkeypair",
                      "-keystore",
                      "/tmp/release.jceks",
                      "-storetype",
                      "JCEKS",
                      "-alias",
                      "signing",
                      "-keyalg",
                      "Ed448",
                      "-validity",
                      "825",
                      "-dname",
                      "CN=Example",
                      "-storepass:file",
                      "/tmp/store-pass",
                      "-keypass:file",
                      "/tmp/key-pass",
                  });
    Platform::WipeSecret(request.StorePassword);
    Platform::WipeSecret(request.KeyPassword);
}

TEST_CASE("ValidateDescribeKeystore requires a keystore file", "[keytool]") {
    REQUIRE(ValidateDescribeKeystore("") == KeytoolRefusal::EmptyPath);
    REQUIRE(ValidateDescribeKeystore("/tmp/coredeck-keytool-does-not-exist.p12") == KeytoolRefusal::MissingFile);
}

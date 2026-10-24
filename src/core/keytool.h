//
// Created by AbdulMuaz Aqeel on 16/10/2026.
//

#ifndef COREDECK_KEYTOOL_H
#define COREDECK_KEYTOOL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/jdk.h"

namespace CoreDeck::Core {
    constexpr int KEYSTORE_PASSWORD_MIN = 6;
    constexpr int KEYSTORE_COUNTRY_CODE_LENGTH = 2;
    constexpr int KEYSTORE_VALIDITY_MIN_DAYS = 1;
    constexpr int KEYSTORE_VALIDITY_MAX_DAYS = 365000;
    constexpr int DEFAULT_KEY_VALIDITY_DAYS = 3650;
    constexpr int DEFAULT_KEY_SIZE = 2048;
    constexpr int DEFAULT_EC_KEY_SIZE = 256;

    enum class KeytoolAliasStatus : uint8_t {
        Ok,
        Refused,
        Failed,
    };

    struct KeytoolAliasListResult {
        KeytoolAliasStatus Status = KeytoolAliasStatus::Failed;
        bool ToolMissing = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        std::vector<std::string> Aliases;
    };

    enum class KeytoolStatus : uint8_t {
        Ok,
        Refused,
        Failed,
    };

    enum class KeytoolRefusal : uint8_t {
        None,
        MissingKeytool,
        EmptyPath,
        MissingFile,
        PathExists,
        ParentMissing,
        EmptyAlias,
        PasswordTooShort,
        KeyPasswordTooShort,
        KeyPasswordDiffers,
        InvalidValidity,
        InvalidKeySize,
        InvalidCountry,
        EmptyDistinguishedName,
        PasswordFileFailed,
    };

    enum class KeystoreType : uint8_t {
        Pkcs12,
        Jks,
        Jceks,
    };

    enum class KeyAlgorithm : uint8_t {
        Rsa,
        Ec,
        Dsa,
        Ed25519,
        Ed448,
    };

    struct KeystoreFingerprint {
        std::string Algorithm;
        std::string Value;
    };

    struct KeystoreCertificate {
        std::string Owner;
        std::string Issuer;
        std::string SerialNumber;
        std::string ValidFrom;
        std::string ValidUntil;
        std::string SignatureAlgorithm;
        std::string PublicKey;
        std::vector<KeystoreFingerprint> Fingerprints;
    };

    struct KeystoreEntry {
        std::string Alias;
        std::string CreationDate;
        std::string EntryType;
        std::vector<KeystoreCertificate> Certificates;
    };

    struct KeystoreDescription {
        KeytoolStatus Status = KeytoolStatus::Failed;
        KeytoolRefusal Refusal = KeytoolRefusal::None;
        bool ToolMissing = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        std::string StoreType;
        std::string Provider;
        std::vector<KeystoreEntry> Entries;
    };

    struct CreateKeystoreRequest {
        std::string KeystorePath;
        KeystoreType StoreType = KeystoreType::Pkcs12;
        std::string Alias;
        KeyAlgorithm Algorithm = KeyAlgorithm::Rsa;
        int KeySize = DEFAULT_KEY_SIZE;
        int ValidityDays = DEFAULT_KEY_VALIDITY_DAYS;
        std::string StorePassword;
        std::string KeyPassword;
        std::string CommonName;
        std::string OrganizationalUnit;
        std::string Organization;
        std::string Locality;
        std::string State;
        std::string Country;
        JdkInfo Jdk;
        Platform::EnvVars ToolEnv;
    };

    struct CreateKeystoreResult {
        KeytoolStatus Status = KeytoolStatus::Failed;
        KeytoolRefusal Refusal = KeytoolRefusal::None;
        bool ToolMissing = false;
        bool Started = false;
        int ExitCode = -1;
        std::string Output;
        KeystoreDescription Description;
    };

    class KeystorePasswordFile {
    public:
        KeystorePasswordFile() = default;
        ~KeystorePasswordFile();

        KeystorePasswordFile(const KeystorePasswordFile &) = delete;
        KeystorePasswordFile(KeystorePasswordFile &&) = delete;
        KeystorePasswordFile &operator=(const KeystorePasswordFile &) = delete;
        KeystorePasswordFile &operator=(KeystorePasswordFile &&) = delete;

        bool Write(const std::string &secret);

        void Destroy();

        const std::filesystem::path &Path() const;

    private:
        std::filesystem::path m_FilePath;
        std::size_t m_Bytes = 0;
    };

    std::string KeytoolPath(const JdkInfo &jdk);

    const char *KeystoreTypeName(KeystoreType type);

    const char *KeyAlgorithmName(KeyAlgorithm algorithm);

    bool KeyAlgorithmUsesSize(KeyAlgorithm algorithm);

    int DefaultKeySize(KeyAlgorithm algorithm);

    std::string_view KeystoreTypeFromMagic(const std::uint8_t *bytes, std::size_t size);

    std::string DetectKeystoreType(const std::string &keystorePath);

    std::vector<std::string> KeystoreFilePatterns();

    std::vector<std::string> ParseKeystoreAliases(const std::string &output);

    KeystoreDescription ParseKeystoreDescription(const std::string &output);

    bool KeystorePasswordRejected(const std::string &output);

    std::string BuildDistinguishedName(const CreateKeystoreRequest &request);

    KeytoolRefusal ValidateDescribeKeystore(const std::string &keystorePath);

    KeytoolRefusal ValidateCreateKeystore(const CreateKeystoreRequest &request);

    std::vector<std::string> BuildCreateKeystoreArguments(
        const CreateKeystoreRequest &request,
        const std::string &storePassFile,
        const std::string &keyPassFile
    );

    KeytoolAliasListResult ListKeystoreAliases(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const Platform::EnvVars &toolEnv
    );

    KeystoreDescription DescribeKeystore(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const Platform::EnvVars &toolEnv
    );

    CreateKeystoreResult CreateKeystore(CreateKeystoreRequest request);
}

#endif // COREDECK_KEYTOOL_H

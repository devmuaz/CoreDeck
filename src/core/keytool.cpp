//
// Created by AbdulMuaz Aqeel on 16/10/2026.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
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

#include "keytool.h"
#include "platform/paths.h"
#include "platform/process.h"
#include "platform/utilities.h"

namespace CoreDeck::Core {
    namespace {
        constexpr std::array<std::uint8_t, 4> JKS_MAGIC{0xFE, 0xED, 0xFE, 0xED};
        constexpr std::array<std::uint8_t, 4> JCEKS_MAGIC{0xCE, 0xCE, 0xCE, 0xCE};

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
                const auto ticks = static_cast<std::uint64_t>(
                    std::chrono::steady_clock::now().time_since_epoch().count()
                );
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
            if (error) {
                return {};
            }
            return directory;
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

        struct RemovedFile {
            std::filesystem::path Path;
            bool Armed = false;

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

        struct KeytoolInvocation {
            KeytoolStatus Status = KeytoolStatus::Failed;
            KeytoolRefusal Refusal = KeytoolRefusal::None;
            bool ToolMissing = false;
            bool Started = false;
            int ExitCode = -1;
            std::string Output;
        };

        bool StartsWith(const std::string &line, const std::string &label) {
            return line.size() >= label.size() && line.starts_with(label);
        }

        std::string ValueAfter(const std::string &line, const std::string &label) {
            if (!StartsWith(line, label)) {
                return {};
            }
            return TrimCopy(line.substr(label.size()));
        }

        bool IsAsteriskLine(const std::string &line) {
            return !line.empty() && std::ranges::all_of(line, [](const char character) {
                return character == '*';
            });
        }

        bool CertificateHasContent(const KeystoreCertificate &certificate) {
            return !certificate.Owner.empty() || !certificate.Issuer.empty() || !certificate.SerialNumber.empty() || !certificate.Fingerprints.empty();
        }

        KeystoreCertificate &OpenCertificate(KeystoreEntry &entry, const bool fresh) {
            if (entry.Certificates.empty() || (fresh && CertificateHasContent(entry.Certificates.back()))) {
                entry.Certificates.emplace_back();
            }
            return entry.Certificates.back();
        }

        bool ParseFingerprint(const std::string &line, KeystoreFingerprint &fingerprint) {
            const auto colon = line.find(':');
            if (colon == std::string::npos || colon == 0) {
                return false;
            }
            std::string name = TrimCopy(line.substr(0, colon));
            if (name.find("SHA") == std::string::npos && name.find("MD5") == std::string::npos) {
                return false;
            }
            std::string value = TrimCopy(line.substr(colon + 1));
            if (value.empty()) {
                return false;
            }
            fingerprint.Algorithm = std::move(name);
            fingerprint.Value = std::move(value);
            return true;
        }

        void ApplyCertificateLine(KeystoreEntry &entry, const std::string &line) {
            if (StartsWith(line, "Certificate[")) {
                OpenCertificate(entry, true);
                return;
            }
            if (StartsWith(line, "Owner:")) {
                const bool next = !entry.Certificates.empty() && !entry.Certificates.back().Owner.empty();
                OpenCertificate(entry, next).Owner = ValueAfter(line, "Owner:");
                return;
            }

            KeystoreFingerprint fingerprint;
            const bool isFingerprint = ParseFingerprint(line, fingerprint);
            const bool known = StartsWith(line, "Issuer:") || StartsWith(line, "Serial number:") || StartsWith(line, "Valid from:") || StartsWith(line, "Signature algorithm name:") || StartsWith(line, "Subject Public Key Algorithm:");
            if (!known && !isFingerprint) {
                return;
            }

            KeystoreCertificate &certificate = OpenCertificate(entry, false);
            if (StartsWith(line, "Issuer:")) {
                certificate.Issuer = ValueAfter(line, "Issuer:");
                return;
            }
            if (StartsWith(line, "Serial number:")) {
                certificate.SerialNumber = ValueAfter(line, "Serial number:");
                return;
            }
            if (StartsWith(line, "Valid from:")) {
                constexpr std::string_view FROM = "Valid from:";
                constexpr std::string_view UNTIL = " until:";
                const auto until = line.find(UNTIL);
                if (until == std::string::npos) {
                    certificate.ValidFrom = ValueAfter(line, std::string(FROM));
                    return;
                }
                certificate.ValidFrom = TrimCopy(line.substr(FROM.size(), until - FROM.size()));
                certificate.ValidUntil = TrimCopy(line.substr(until + UNTIL.size()));
                return;
            }
            if (StartsWith(line, "Signature algorithm name:")) {
                certificate.SignatureAlgorithm = ValueAfter(line, "Signature algorithm name:");
                return;
            }
            if (StartsWith(line, "Subject Public Key Algorithm:")) {
                certificate.PublicKey = ValueAfter(line, "Subject Public Key Algorithm:");
                return;
            }
            if (isFingerprint) {
                certificate.Fingerprints.push_back(std::move(fingerprint));
            }
        }

        bool MagicEquals(const std::uint8_t *bytes, const std::size_t size, const std::array<std::uint8_t, 4> &magic) {
            return size >= magic.size() && std::equal(magic.begin(), magic.end(), bytes);
        }

        bool IsAsciiAlpha(const unsigned char character) {
            return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        }

        char UpperAscii(const unsigned char character) {
            if (character >= 'a' && character <= 'z') {
                return static_cast<char>(character - ('a' - 'A'));
            }
            return static_cast<char>(character);
        }

        std::string CountryValue(const std::string &country) {
            std::string value = TrimCopy(country);
            if (value.size() == 2 && IsAsciiAlpha(static_cast<unsigned char>(value.at(0))) && IsAsciiAlpha(static_cast<unsigned char>(value.at(1)))) {
                value.at(0) = UpperAscii(static_cast<unsigned char>(value.at(0)));
                value.at(1) = UpperAscii(static_cast<unsigned char>(value.at(1)));
            }
            return value;
        }

        std::string EscapeDnValue(const std::string &value) {
            std::string escaped;
            escaped.reserve(value.size());
            for (const unsigned char character: value) {
                if (character == '\\' || character == ',' || character == '+' || character == '"' || character == '<' || character == '>' || character == ';') {
                    escaped.push_back('\\');
                }
                escaped.push_back(static_cast<char>(character));
            }
            return escaped;
        }

        void AppendAva(std::string &dname, const char *key, const std::string &raw) {
            const std::string value = TrimCopy(raw);
            if (value.empty()) {
                return;
            }
            if (!dname.empty()) {
                dname += ", ";
            }
            dname += key;
            dname += '=';
            dname += EscapeDnValue(value);
        }

        bool KeySizeValid(const KeyAlgorithm algorithm, const int size) {
            switch (algorithm) {
                case KeyAlgorithm::Rsa:
                    return size >= 1024 && size <= 8192 && (size % 8) == 0;
                case KeyAlgorithm::Dsa:
                    return size == 1024 || size == 2048 || size == 3072;
                case KeyAlgorithm::Ec:
                    return size == 256 || size == 384 || size == 521;
                case KeyAlgorithm::Ed25519:
                case KeyAlgorithm::Ed448:
                    return true;
            }
            return false;
        }

        void AppendStoreType(std::vector<std::string> &args, const std::string &keystorePath) {
            const std::string type = DetectKeystoreType(keystorePath);
            if (type.empty()) {
                return;
            }
            args.push_back("-storetype");
            args.push_back(type);
        }

        std::vector<std::string> BuildListArguments(const std::string &keystorePath, const std::string &storePassFile, const bool verbose) {
            std::vector<std::string> args = {
                "-J-Duser.language=en",
                "-J-Duser.country=US",
                "-list",
            };
            if (verbose) {
                args.emplace_back("-v");
            }
            args.push_back("-keystore");
            args.push_back(keystorePath);
            AppendStoreType(args, keystorePath);
            args.push_back("-storepass:file");
            args.push_back(storePassFile);
            return args;
        }

        KeytoolInvocation RunKeytool(const JdkInfo &jdk, const std::vector<std::string> &args, const Platform::EnvVars &toolEnv) {
            KeytoolInvocation result;
            const std::string keytool = KeytoolPath(jdk);
            if (jdk.JavaHome.empty() || !IsRegularFile(keytool)) {
                result.Status = KeytoolStatus::Refused;
                result.Refusal = KeytoolRefusal::MissingKeytool;
                result.ToolMissing = true;
                return result;
            }

            const Platform::CommandResult command = Platform::RunCommandArgsResult(keytool, args, "", toolEnv);
            result.Started = command.Started;
            result.ExitCode = command.ExitCode;
            result.Output = command.Output;
            if (CommandFailed(command)) {
                result.Status = KeytoolStatus::Failed;
                return result;
            }
            result.Status = KeytoolStatus::Ok;
            return result;
        }

        void CopyInvocation(const KeytoolInvocation &invocation, KeystoreDescription &description) {
            description.Status = invocation.Status;
            description.Refusal = invocation.Refusal;
            description.ToolMissing = invocation.ToolMissing;
            description.Started = invocation.Started;
            description.ExitCode = invocation.ExitCode;
            description.Output = invocation.Output;
        }
    }

    KeystorePasswordFile::~KeystorePasswordFile() {
        Destroy();
    }

    const std::filesystem::path &KeystorePasswordFile::Path() const {
        return m_FilePath;
    }

    bool KeystorePasswordFile::Write(const std::string &secret) {
        Destroy();
        const std::filesystem::path directory = TempDirectory();
        if (directory.empty()) {
            return false;
        }

        int fd = -1;
        for (int attempt = 0; attempt < 8 && fd < 0; ++attempt) {
            m_FilePath = directory / ("coredeck-ks-" + RandomToken());
            fd = OpenExclusive(m_FilePath);
        }
        if (fd < 0) {
            m_FilePath.clear();
            return false;
        }

        m_Bytes = secret.size() + 1;
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

    void KeystorePasswordFile::Destroy() {
        if (!m_FilePath.empty()) {
            const int fd = OpenForWipe(m_FilePath);
            if (fd >= 0) {
                if (OverwriteWithZeros(fd, m_Bytes)) {
                    SyncFd(fd);
                }
                CloseFd(fd);
            }
            RemoveQuiet(m_FilePath);
        }
        m_FilePath.clear();
        m_Bytes = 0;
    }

    std::string KeytoolPath(const JdkInfo &jdk) {
        return JdkExecutablePath(jdk, "keytool");
    }

    const char *KeystoreTypeName(const KeystoreType type) {
        switch (type) {
            case KeystoreType::Pkcs12:
                return "PKCS12";
            case KeystoreType::Jks:
                return "JKS";
            case KeystoreType::Jceks:
                return "JCEKS";
        }
        return "PKCS12";
    }

    const char *KeyAlgorithmName(const KeyAlgorithm algorithm) {
        switch (algorithm) {
            case KeyAlgorithm::Rsa:
                return "RSA";
            case KeyAlgorithm::Ec:
                return "EC";
            case KeyAlgorithm::Dsa:
                return "DSA";
            case KeyAlgorithm::Ed25519:
                return "Ed25519";
            case KeyAlgorithm::Ed448:
                return "Ed448";
        }
        return "RSA";
    }

    bool KeyAlgorithmUsesSize(const KeyAlgorithm algorithm) {
        return algorithm == KeyAlgorithm::Rsa || algorithm == KeyAlgorithm::Ec || algorithm == KeyAlgorithm::Dsa;
    }

    int DefaultKeySize(const KeyAlgorithm algorithm) {
        switch (algorithm) {
            case KeyAlgorithm::Ec:
                return DEFAULT_EC_KEY_SIZE;
            case KeyAlgorithm::Rsa:
            case KeyAlgorithm::Dsa:
                return DEFAULT_KEY_SIZE;
            case KeyAlgorithm::Ed25519:
            case KeyAlgorithm::Ed448:
                break;
        }
        return 0;
    }

    std::string_view KeystoreTypeFromMagic(const std::uint8_t *bytes, const std::size_t size) {
        if (bytes == nullptr || size == 0) {
            return {};
        }
        if (MagicEquals(bytes, size, JKS_MAGIC)) {
            return "JKS";
        }
        if (MagicEquals(bytes, size, JCEKS_MAGIC)) {
            return "JCEKS";
        }
        if (bytes[0] == 0x30) {
            return "PKCS12";
        }
        return {};
    }

    std::string DetectKeystoreType(const std::string &keystorePath) {
        std::ifstream input(std::filesystem::path(keystorePath), std::ios::binary);
        if (!input) {
            return {};
        }
        std::array<std::uint8_t, 4> bytes{};
        input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return std::string(KeystoreTypeFromMagic(bytes.data(), static_cast<std::size_t>(input.gcount())));
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

    KeystoreDescription ParseKeystoreDescription(const std::string &output) {
        KeystoreDescription description;
        description.Status = KeytoolStatus::Ok;
        KeystoreEntry current;
        bool inEntry = false;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            line = TrimCopy(std::move(line));
            if (line.empty() || IsAsteriskLine(line)) {
                continue;
            }
            if (StartsWith(line, "Certificate fingerprints:") || StartsWith(line, "Certificate chain length:") || StartsWith(line, "Your keystore contains") || StartsWith(line, "Version:")) {
                continue;
            }
            if (const std::string storeType = ValueAfter(line, "Keystore type:"); !storeType.empty() && !inEntry) {
                description.StoreType = storeType;
                continue;
            }
            if (const std::string provider = ValueAfter(line, "Keystore provider:"); !provider.empty() && !inEntry) {
                description.Provider = provider;
                continue;
            }
            if (StartsWith(line, "Alias name:")) {
                if (inEntry) {
                    description.Entries.push_back(std::move(current));
                    current = {};
                }
                inEntry = true;
                current.Alias = ValueAfter(line, "Alias name:");
                continue;
            }
            if (!inEntry) {
                continue;
            }
            if (StartsWith(line, "Creation date:")) {
                current.CreationDate = ValueAfter(line, "Creation date:");
                continue;
            }
            if (StartsWith(line, "Entry type:")) {
                current.EntryType = ValueAfter(line, "Entry type:");
                continue;
            }
            ApplyCertificateLine(current, line);
        }
        if (inEntry) {
            description.Entries.push_back(std::move(current));
        }
        return description;
    }

    bool KeystorePasswordRejected(const std::string &output) {
        return Platform::ContainsIgnoreCase(output, "password was incorrect");
    }

    std::string BuildDistinguishedName(const CreateKeystoreRequest &request) {
        std::string dname;
        AppendAva(dname, "CN", request.CommonName);
        AppendAva(dname, "OU", request.OrganizationalUnit);
        AppendAva(dname, "O", request.Organization);
        AppendAva(dname, "L", request.Locality);
        AppendAva(dname, "ST", request.State);
        AppendAva(dname, "C", CountryValue(request.Country));
        return dname;
    }

    KeytoolRefusal ValidateDescribeKeystore(const std::string &keystorePath) {
        if (TrimCopy(keystorePath).empty()) {
            return KeytoolRefusal::EmptyPath;
        }
        if (!IsRegularFile(keystorePath)) {
            return KeytoolRefusal::MissingFile;
        }
        return KeytoolRefusal::None;
    }

    KeytoolRefusal ValidateCreateKeystore(const CreateKeystoreRequest &request) {
        const std::string path = TrimCopy(request.KeystorePath);
        if (path.empty()) {
            return KeytoolRefusal::EmptyPath;
        }
        std::error_code error;
        const std::filesystem::path file(path);
        if (std::filesystem::exists(file, error)) {
            return KeytoolRefusal::PathExists;
        }
        const std::filesystem::path parent = file.parent_path();
        if (!parent.empty() && !std::filesystem::is_directory(parent, error)) {
            return KeytoolRefusal::ParentMissing;
        }
        if (TrimCopy(request.Alias).empty()) {
            return KeytoolRefusal::EmptyAlias;
        }
        if (request.StorePassword.size() < static_cast<std::size_t>(KEYSTORE_PASSWORD_MIN)) {
            return KeytoolRefusal::PasswordTooShort;
        }
        const bool distinctKey = !request.KeyPassword.empty() && request.KeyPassword != request.StorePassword;
        if (distinctKey && request.StoreType == KeystoreType::Pkcs12) {
            return KeytoolRefusal::KeyPasswordDiffers;
        }
        if (distinctKey && request.KeyPassword.size() < static_cast<std::size_t>(KEYSTORE_PASSWORD_MIN)) {
            return KeytoolRefusal::KeyPasswordTooShort;
        }
        if (request.ValidityDays < KEYSTORE_VALIDITY_MIN_DAYS) {
            return KeytoolRefusal::InvalidValidity;
        }
        if (!KeySizeValid(request.Algorithm, request.KeySize)) {
            return KeytoolRefusal::InvalidKeySize;
        }
        const std::string country = TrimCopy(request.Country);
        if (!country.empty() && CountryValue(country).size() != KEYSTORE_COUNTRY_CODE_LENGTH) {
            return KeytoolRefusal::InvalidCountry;
        }
        if (BuildDistinguishedName(request).empty()) {
            return KeytoolRefusal::EmptyDistinguishedName;
        }
        return KeytoolRefusal::None;
    }

    std::vector<std::string> BuildCreateKeystoreArguments(
        const CreateKeystoreRequest &request,
        const std::string &storePassFile,
        const std::string &keyPassFile
    ) {
        std::vector<std::string> args = {
            "-J-Duser.language=en",
            "-J-Duser.country=US",
            "-genkeypair",
            "-keystore",
            TrimCopy(request.KeystorePath),
            "-storetype",
            KeystoreTypeName(request.StoreType),
            "-alias",
            TrimCopy(request.Alias),
            "-keyalg",
            KeyAlgorithmName(request.Algorithm),
        };
        if (KeyAlgorithmUsesSize(request.Algorithm) && request.KeySize > 0) {
            args.emplace_back("-keysize");
            args.push_back(std::to_string(request.KeySize));
        }
        args.emplace_back("-validity");
        args.push_back(std::to_string(request.ValidityDays));
        args.emplace_back("-dname");
        args.push_back(BuildDistinguishedName(request));
        args.emplace_back("-storepass:file");
        args.push_back(storePassFile);
        if (!keyPassFile.empty()) {
            args.emplace_back("-keypass:file");
            args.push_back(keyPassFile);
        }
        return args;
    }

    KeytoolAliasListResult ListKeystoreAliases(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const Platform::EnvVars &toolEnv
    ) {
        SecretString password{std::move(storePassword)};
        Platform::WipeSecret(storePassword);

        KeytoolAliasListResult result;
        if (keystorePath.empty()) {
            result.Status = KeytoolAliasStatus::Refused;
            return result;
        }
        if (jdk.JavaHome.empty() || !IsRegularFile(KeytoolPath(jdk))) {
            result.Status = KeytoolAliasStatus::Refused;
            result.ToolMissing = true;
            return result;
        }

        KeystorePasswordFile passFile;
        if (!passFile.Write(password.Value)) {
            result.Status = KeytoolAliasStatus::Failed;
            return result;
        }

        const KeytoolInvocation invocation = RunKeytool(jdk, BuildListArguments(keystorePath, passFile.Path().string(), false), toolEnv);
        passFile.Destroy();
        Platform::WipeSecret(password.Value);

        result.ToolMissing = invocation.ToolMissing;
        result.Started = invocation.Started;
        result.ExitCode = invocation.ExitCode;
        result.Output = invocation.Output;
        if (invocation.Status == KeytoolStatus::Refused) {
            result.Status = KeytoolAliasStatus::Refused;
            return result;
        }
        if (invocation.Status != KeytoolStatus::Ok) {
            result.Status = KeytoolAliasStatus::Failed;
            return result;
        }
        result.Status = KeytoolAliasStatus::Ok;
        result.Aliases = ParseKeystoreAliases(invocation.Output);
        return result;
    }

    KeystoreDescription DescribeKeystore(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const Platform::EnvVars &toolEnv
    ) {
        SecretString password{std::move(storePassword)};
        Platform::WipeSecret(storePassword);

        KeystoreDescription description;
        if (const KeytoolRefusal refusal = ValidateDescribeKeystore(keystorePath); refusal != KeytoolRefusal::None) {
            description.Status = KeytoolStatus::Refused;
            description.Refusal = refusal;
            return description;
        }
        if (jdk.JavaHome.empty() || !IsRegularFile(KeytoolPath(jdk))) {
            description.Status = KeytoolStatus::Refused;
            description.Refusal = KeytoolRefusal::MissingKeytool;
            description.ToolMissing = true;
            return description;
        }

        KeystorePasswordFile passFile;
        if (!passFile.Write(password.Value)) {
            description.Status = KeytoolStatus::Failed;
            description.Refusal = KeytoolRefusal::PasswordFileFailed;
            return description;
        }

        const KeytoolInvocation invocation = RunKeytool(jdk, BuildListArguments(keystorePath, passFile.Path().string(), true), toolEnv);
        passFile.Destroy();
        Platform::WipeSecret(password.Value);
        CopyInvocation(invocation, description);
        if (invocation.Status != KeytoolStatus::Ok) {
            return description;
        }

        KeystoreDescription parsed = ParseKeystoreDescription(invocation.Output);
        description.StoreType = std::move(parsed.StoreType);
        description.Provider = std::move(parsed.Provider);
        description.Entries = std::move(parsed.Entries);
        return description;
    }

    CreateKeystoreResult CreateKeystore(CreateKeystoreRequest request) {
        SecretString storePassword{std::move(request.StorePassword)};
        Platform::WipeSecret(request.StorePassword);
        SecretString keyPassword{std::move(request.KeyPassword)};
        Platform::WipeSecret(request.KeyPassword);
        CreateKeystoreResult result;
        const std::string keytool = KeytoolPath(request.Jdk);
        if (request.Jdk.JavaHome.empty() || !IsRegularFile(keytool)) {
            result.Status = KeytoolStatus::Refused;
            result.Refusal = KeytoolRefusal::MissingKeytool;
            result.ToolMissing = true;
            return result;
        }

        request.StorePassword = storePassword.Value;
        request.KeyPassword = keyPassword.Value;
        const KeytoolRefusal refusal = ValidateCreateKeystore(request);
        Platform::WipeSecret(request.StorePassword);
        Platform::WipeSecret(request.KeyPassword);
        if (refusal != KeytoolRefusal::None) {
            result.Status = KeytoolStatus::Refused;
            result.Refusal = refusal;
            return result;
        }

        KeystorePasswordFile storePassFile;
        if (!storePassFile.Write(storePassword.Value)) {
            result.Status = KeytoolStatus::Failed;
            result.Refusal = KeytoolRefusal::PasswordFileFailed;
            return result;
        }

        const bool distinctKey = !keyPassword.Value.empty() && keyPassword.Value != storePassword.Value;
        KeystorePasswordFile keyPassFile;
        std::string keyPassPath;
        if (distinctKey) {
            if (!keyPassFile.Write(keyPassword.Value)) {
                result.Status = KeytoolStatus::Failed;
                result.Refusal = KeytoolRefusal::PasswordFileFailed;
                return result;
            }
            keyPassPath = keyPassFile.Path().string();
        }

        RemovedFile partial;
        partial.Path = TrimCopy(request.KeystorePath);
        const KeytoolInvocation invocation = RunKeytool(
            request.Jdk,
            BuildCreateKeystoreArguments(request, storePassFile.Path().string(), keyPassPath),
            request.ToolEnv
        );
        if (invocation.Status != KeytoolStatus::Ok) {
            partial.Armed = invocation.Started;
            storePassFile.Destroy();
            keyPassFile.Destroy();
            Platform::WipeSecret(storePassword.Value);
            Platform::WipeSecret(keyPassword.Value);
            result.Status = invocation.Status;
            result.Refusal = invocation.Refusal;
            result.ToolMissing = invocation.ToolMissing;
            result.Started = invocation.Started;
            result.ExitCode = invocation.ExitCode;
            result.Output = invocation.Output;
            return result;
        }

        result.Description = DescribeKeystore(request.Jdk, partial.Path.string(), storePassword.Value, request.ToolEnv);
        storePassFile.Destroy();
        keyPassFile.Destroy();
        Platform::WipeSecret(storePassword.Value);
        Platform::WipeSecret(keyPassword.Value);

        result.Status = KeytoolStatus::Ok;
        result.Started = true;
        result.ExitCode = invocation.ExitCode;
        result.Output = invocation.Output;
        return result;
    }
}

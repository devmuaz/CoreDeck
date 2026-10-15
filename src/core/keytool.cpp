//
// Created by AbdulMuaz Aqeel on 16/10/2026.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
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
        if (jdk.JavaHome.empty()) {
            return {};
        }
        return Paths::JoinPaths({jdk.JavaHome, "bin", "keytool" + Paths::GetExecutableExtension()});
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

    KeytoolAliasListResult ListKeystoreAliases(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const EnvVars &toolEnv
    ) {
        SecretString password{std::move(storePassword)};
        WipeSecret(storePassword);

        KeytoolAliasListResult result;
        const std::string keytool = KeytoolPath(jdk);
        if (jdk.JavaHome.empty() || !IsRegularFile(keytool)) {
            result.Status = KeytoolAliasStatus::Refused;
            result.ToolMissing = true;
            return result;
        }
        if (keystorePath.empty()) {
            result.Status = KeytoolAliasStatus::Refused;
            return result;
        }

        KeystorePasswordFile passFile;
        if (!passFile.Write(password.Value)) {
            result.Status = KeytoolAliasStatus::Failed;
            return result;
        }

        const CommandResult command = RunCommandArgsResult(
            keytool,
            {"-list", "-keystore", keystorePath, "-storepass:file", passFile.Path().string()},
            "",
            toolEnv
        );
        passFile.Destroy();
        WipeSecret(password.Value);

        result.Started = command.Started;
        result.ExitCode = command.ExitCode;
        result.Output = command.Output;
        if (CommandFailed(command)) {
            result.Status = KeytoolAliasStatus::Failed;
            return result;
        }
        result.Status = KeytoolAliasStatus::Ok;
        result.Aliases = ParseKeystoreAliases(command.Output);
        return result;
    }
}

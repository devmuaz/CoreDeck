//
// Created by AbdulMuaz Aqeel on 16/10/2026.
//

#ifndef COREDECK_KEYTOOL_H
#define COREDECK_KEYTOOL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "jdk.h"

namespace CoreDeck {
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

    std::vector<std::string> ParseKeystoreAliases(const std::string &output);

    KeytoolAliasListResult ListKeystoreAliases(
        const JdkInfo &jdk,
        const std::string &keystorePath,
        std::string storePassword,
        const EnvVars &toolEnv
    );
}

#endif // COREDECK_KEYTOOL_H

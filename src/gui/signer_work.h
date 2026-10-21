//
// Created by AbdulMuaz Aqeel on 21/10/2026.
//

#ifndef COREDECK_SIGNER_WORK_H
#define COREDECK_SIGNER_WORK_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "../core/apk_signer.h"
#include "../core/jar_signer.h"
#include "../core/keytool.h"

namespace CoreDeck {
    enum class SignerTask : uint8_t {
        None,
        Verify,
        Aliases,
        Sign,
    };

    using ApkSignerTask = SignerTask;
    using JarSignerTask = SignerTask;

    template<typename VerifyT, typename SignT>
    struct SignerWork {
        std::atomic<bool> Busy{false};
        SignerTask Task = SignerTask::None;

        std::future<VerifyT> VerifyFuture;
        std::future<KeytoolAliasListResult> AliasFuture;
        std::future<SignT> SignFuture;
        bool AliasRefreshPending = false;
        bool AliasListScheduled = false;
        std::chrono::steady_clock::time_point AliasListDue;

        std::string InputPath;
        std::string KeystorePath;
        std::string OutputPath;
        bool OutputCustomized = false;

        char StorePassword[512] = {};
        char KeyPassword[512] = {};
        bool ShowStorePassword = false;
        bool ShowKeyPassword = false;
        std::uint32_t SecretEpoch = 0;
        bool KeyPasswordMatches = true;

        bool HasVerify = false;
        VerifyT Verify;

        bool HasAliasResult = false;
        KeytoolAliasListResult AliasResult;
        std::vector<std::string> Aliases;
        std::string SelectedAlias;

        bool HasSignResult = false;
        SignT SignResult;
        std::string KeystoreType;
    };

    struct ApkSignerWork : SignerWork<ApkVerifyResult, ApkSignResult> {
        bool Align = true;
    };

    struct JarSignerWork : SignerWork<JarVerifyResult, JarSignResult> {};
}

#endif // COREDECK_SIGNER_WORK_H

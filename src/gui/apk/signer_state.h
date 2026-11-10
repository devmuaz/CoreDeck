//
// Created by AbdulMuaz Aqeel on 21/10/2026.
//

#ifndef COREDECK_GUI_APK_SIGNER_STATE_H
#define COREDECK_GUI_APK_SIGNER_STATE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "core/apk/apk_signer.h"
#include "core/apk/jar_signer.h"
#include "core/apk/keytool.h"

namespace CoreDeck::Gui {
    enum class SignerTask : uint8_t {
        None,
        Verify,
        Aliases,
        Sign,
    };

    using ApkSignerTask = SignerTask;
    using JarSignerTask = SignerTask;

    template<typename VerifyT, typename SignT>
    struct SignerState {
        std::atomic<bool> Busy{false};
        SignerTask Task = SignerTask::None;

        std::future<VerifyT> VerifyFuture;
        std::future<Core::KeytoolAliasListResult> AliasFuture;
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
        Core::KeytoolAliasListResult AliasResult;
        std::vector<std::string> Aliases;
        std::string SelectedAlias;

        bool HasSignResult = false;
        SignT SignResult;
        std::string KeystoreType;
    };

    struct ApkSignerState : SignerState<Core::ApkVerifyResult, Core::ApkSignResult> {
        bool Align = true;
    };

    struct JarSignerState : SignerState<Core::JarVerifyResult, Core::JarSignResult> {};
}

#endif // COREDECK_GUI_APK_SIGNER_STATE_H

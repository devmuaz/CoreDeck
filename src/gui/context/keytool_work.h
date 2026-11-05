//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_KEYTOOL_WORK_H
#define COREDECK_KEYTOOL_WORK_H

#include <atomic>
#include <cstdint>
#include <future>
#include <string>

#include "core/keytool.h"

namespace CoreDeck::Gui {
    enum class KeytoolTask : uint8_t {
        None,
        Inspect,
        Create,
    };

    enum class KeytoolPage : uint8_t {
        Inspect,
        Create,
    };

    struct KeytoolWork {
        std::atomic<bool> Busy{false};
        KeytoolTask Task = KeytoolTask::None;
        KeytoolPage Page = KeytoolPage::Inspect;

        std::future<Core::KeystoreDescription> InspectFuture;
        std::future<Core::CreateKeystoreResult> CreateFuture;

        std::string InspectPath;
        std::string CreatePath;

        char InspectPassword[512] = {};
        char CreatePassword[512] = {};
        char ConfirmPassword[512] = {};
        char KeyPassword[512] = {};
        bool ShowInspectPassword = false;
        bool ShowCreatePassword = false;
        bool ShowConfirmPassword = false;
        bool ShowKeyPassword = false;
        bool KeyPasswordMatches = true;
        std::uint32_t SecretEpoch = 0;

        Core::KeystoreType StoreType = Core::KeystoreType::Pkcs12;
        int CreateExtension = 0;
        int CreateTemplate = 0;
        Core::KeyAlgorithm Algorithm = Core::KeyAlgorithm::Rsa;
        int KeySize = Core::DEFAULT_KEY_SIZE;
        int ValidityDays = Core::DEFAULT_KEY_VALIDITY_DAYS;
        char Alias[256] = {};
        char CommonName[256] = {};
        char OrganizationalUnit[256] = {};
        char Organization[256] = {};
        char Locality[256] = {};
        char StateName[256] = {};
        char Country[8] = {};

        bool HasInspect = false;
        Core::KeystoreDescription InspectResult;
        int InspectEntry = 0;

        bool HasCreate = false;
        Core::CreateKeystoreResult CreateResult;
        int CreateEntry = 0;
    };
}

#endif // COREDECK_KEYTOOL_WORK_H

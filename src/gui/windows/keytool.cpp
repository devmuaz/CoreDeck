//
// Created by AbdulMuaz Aqeel on 16/10/2026.
//

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "keytool.h"
#include "../theme.h"
#include "../utilities.h"
#include "../widgets.h"
#include "../../core/file_dialog.h"
#include "../../core/i18n.h"
#include "../../core/keytool.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    namespace {
        constexpr const char *UNAVAILABLE = "\u2014";

        constexpr const char *STORE_TYPE_LABELS[] = {"PKCS12", "JKS", "JCEKS"};
        constexpr const char *ALGORITHM_LABELS[] = {"RSA", "EC", "DSA", "Ed25519", "Ed448"};
        constexpr const char *KEYSTORE_SUFFIXES[] = {".jks", ".keystore", ".p12", ".pfx", ".jceks"};

        struct CreateTemplateOption {
            const char *Label;
            bool Applies;
            KeystoreType StoreType;
            const char *Extension;
            KeyAlgorithm Algorithm;
            int KeySize;
            int ValidityDays;
            const char *Alias;
            const char *FileName;
            const char *CommonName;
            const char *OrganizationalUnit;
            const char *Organization;
            const char *Locality;
            const char *State;
            const char *Country;
        };

        constexpr CreateTemplateOption CREATE_TEMPLATES[] = {
            {.Label = "None", .Applies = false, .StoreType = KeystoreType::Pkcs12, .Extension = ".jks", .Algorithm = KeyAlgorithm::Rsa, .KeySize = 2048, .ValidityDays = 10000, .Alias = "", .FileName = "", .CommonName = "", .OrganizationalUnit = "", .Organization = "", .Locality = "", .State = "", .Country = ""},
            {.Label = "Flutter Upload", .Applies = true, .StoreType = KeystoreType::Jks, .Extension = ".jks", .Algorithm = KeyAlgorithm::Rsa, .KeySize = 2048, .ValidityDays = 10000, .Alias = "upload", .FileName = "upload-keystore.jks", .CommonName = "", .OrganizationalUnit = "", .Organization = "", .Locality = "", .State = "", .Country = ""},
            {.Label = "Android Release", .Applies = true, .StoreType = KeystoreType::Pkcs12, .Extension = ".jks", .Algorithm = KeyAlgorithm::Rsa, .KeySize = 2048, .ValidityDays = 10000, .Alias = "my-alias", .FileName = "my-release-key.jks", .CommonName = "", .OrganizationalUnit = "", .Organization = "", .Locality = "", .State = "", .Country = ""},
            {.Label = "Android Debug", .Applies = true, .StoreType = KeystoreType::Pkcs12, .Extension = ".keystore", .Algorithm = KeyAlgorithm::Rsa, .KeySize = 2048, .ValidityDays = 10000, .Alias = "androiddebugkey", .FileName = "debug.keystore", .CommonName = "Android Debug", .OrganizationalUnit = "", .Organization = "Android", .Locality = "", .State = "", .Country = "US"},
        };

        struct KeySizeOption {
            int Size;
            const char *Label;
        };

        constexpr KeySizeOption RSA_SIZES[] = {
            {1024, "1024-bit"},
            {2048, "2048-bit"},
            {3072, "3072-bit"},
            {4096, "4096-bit"},
        };

        constexpr KeySizeOption EC_SIZES[] = {
            {256, "256-bit"},
            {384, "384-bit"},
            {521, "521-bit"},
        };

        constexpr KeySizeOption DSA_SIZES[] = {
            {1024, "1024-bit"},
            {2048, "2048-bit"},
            {3072, "3072-bit"},
        };

        struct CreateLaunch {
            CreateKeystoreRequest Request;

            explicit CreateLaunch(CreateKeystoreRequest request) : Request(std::move(request)) {
                WipeSecret(request.StorePassword);
                WipeSecret(request.KeyPassword);
            }

            CreateLaunch(const CreateLaunch &) = delete;
            CreateLaunch &operator=(const CreateLaunch &) = delete;
            CreateLaunch &operator=(CreateLaunch &&) = delete;

            CreateLaunch(CreateLaunch &&other) noexcept : Request(std::move(other.Request)) {
                WipeSecret(other.Request.StorePassword);
                WipeSecret(other.Request.KeyPassword);
            }

            ~CreateLaunch() {
                WipeSecret(Request.StorePassword);
                WipeSecret(Request.KeyPassword);
            }

            CreateKeystoreResult Run() {
                CreateKeystoreResult result = CreateKeystore(std::move(Request));
                WipeSecret(Request.StorePassword);
                WipeSecret(Request.KeyPassword);
                return result;
            }
        };

        struct SecretText {
            std::string Value;

            explicit SecretText(std::string value) : Value(std::move(value)) {
                WipeSecret(value);
            }

            SecretText(const SecretText &) = delete;
            SecretText &operator=(const SecretText &) = delete;
            SecretText &operator=(SecretText &&) = delete;

            SecretText(SecretText &&other) noexcept : Value(std::move(other.Value)) {
                WipeSecret(other.Value);
            }

            ~SecretText() {
                WipeSecret(Value);
            }
        };

        struct RetiredJobs {
            std::vector<std::future<KeystoreDescription>> Inspect;
            std::vector<std::future<CreateKeystoreResult>> Create;
        };

        RetiredJobs &Graveyard() {
            static RetiredJobs jobs;
            return jobs;
        }

        template<typename Result>
        bool TakeFinished(std::future<Result> &job, Result &result, bool &hasResult) {
            if (!job.valid() || job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return false;
            }
            try {
                result = job.get();
            } catch (...) {
                result = {};
                result.Status = KeytoolStatus::Failed;
            }
            hasResult = true;
            return true;
        }

        void RetireJobs(struct Context::KeytoolWork &work) {
            auto &grave = Graveyard();
            Retire(work.InspectFuture, grave.Inspect);
            Retire(work.CreateFuture, grave.Create);
        }

        void PollRetired() {
            auto &grave = Graveyard();
            DropFinished(grave.Inspect);
            DropFinished(grave.Create);
        }

        struct CreateCheck {
            KeytoolRefusal Refusal = KeytoolRefusal::None;
            bool PasswordMismatch = false;

            bool Ready() const {
                return Refusal == KeytoolRefusal::None && !PasswordMismatch;
            }
        };

        void FieldLabel(const char *text) {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(text);
        }

        void SectionLabel(const char *text) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", text);
        }

        int SuffixIndex(const char *suffix) {
            if (suffix == nullptr) {
                return 0;
            }
            for (int index = 0; index < IM_ARRAYSIZE(KEYSTORE_SUFFIXES); ++index) {
                if (std::strcmp(KEYSTORE_SUFFIXES[index], suffix) == 0) {
                    return index;
                }
            }
            return 0;
        }

        void CopyTextField(char *destination, const std::size_t size, const char *source) {
            if (destination == nullptr || size == 0) {
                return;
            }
            const char *text = source != nullptr ? source : "";
            const std::size_t length = std::min(std::strlen(text), size - 1);
            std::memcpy(destination, text, length);
            destination[length] = '\0';
        }

        int ExtensionIndexOf(const std::string &path) {
            const std::string lower = LowerCopy(path);
            for (int index = 0; index < IM_ARRAYSIZE(KEYSTORE_SUFFIXES); ++index) {
                if (lower.ends_with(KEYSTORE_SUFFIXES[index])) {
                    return index;
                }
            }
            return -1;
        }

        bool HasKeystoreSuffix(const std::string &path) {
            return ExtensionIndexOf(path) >= 0;
        }

        const char *CreateExtensionOf(const struct Context::KeytoolWork &work) {
            const int last = IM_ARRAYSIZE(KEYSTORE_SUFFIXES) - 1;
            return KEYSTORE_SUFFIXES[std::clamp(work.CreateExtension, 0, last)];
        }

        std::string ApplyCreateExtension(std::string path, const char *extension) {
            if (path.empty()) {
                return path;
            }
            const int existing = ExtensionIndexOf(path);
            if (existing >= 0) {
                path.resize(path.size() - std::strlen(KEYSTORE_SUFFIXES[existing]));
            }
            path += extension;
            return path;
        }

        void AssignCreatePath(struct Context::KeytoolWork &work, std::string path) {
            const int existing = ExtensionIndexOf(path);
            if (existing >= 0) {
                work.CreateExtension = existing;
                work.CreatePath = std::move(path);
                return;
            }
            work.CreatePath = path + CreateExtensionOf(work);
        }

        void ClearCreate(struct Context::KeytoolWork &work);

        std::string SuggestedKeystore(const struct Context::KeytoolWork &work) {
            if (!work.CreatePath.empty()) {
                return work.CreatePath;
            }
            const int index = std::clamp(work.CreateTemplate, 0, IM_ARRAYSIZE(CREATE_TEMPLATES) - 1);
            const CreateTemplateOption &option = CREATE_TEMPLATES[index];
            if (option.Applies && option.FileName != nullptr && option.FileName[0] != '\0') {
                return option.FileName;
            }
            return std::string("keystore") + CreateExtensionOf(work);
        }

        void ApplyCreateTemplate(struct Context::KeytoolWork &work, const int index) {
            const int selected = std::clamp(index, 0, IM_ARRAYSIZE(CREATE_TEMPLATES) - 1);
            work.CreateTemplate = selected;
            ClearCreate(work);
            const CreateTemplateOption &option = CREATE_TEMPLATES[selected];
            if (!option.Applies) {
                return;
            }
            work.StoreType = option.StoreType;
            work.CreateExtension = SuffixIndex(option.Extension);
            work.Algorithm = option.Algorithm;
            work.KeySize = option.KeySize;
            work.ValidityDays = option.ValidityDays;
            CopyTextField(work.Alias, sizeof work.Alias, option.Alias);
            CopyTextField(work.CommonName, sizeof work.CommonName, option.CommonName);
            CopyTextField(work.OrganizationalUnit, sizeof work.OrganizationalUnit, option.OrganizationalUnit);
            CopyTextField(work.Organization, sizeof work.Organization, option.Organization);
            CopyTextField(work.Locality, sizeof work.Locality, option.Locality);
            CopyTextField(work.StateName, sizeof work.StateName, option.State);
            CopyTextField(work.Country, sizeof work.Country, option.Country);
            work.KeyPasswordMatches = true;
            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.ShowKeyPassword = false;
            work.SecretEpoch += 1;
            if (!work.CreatePath.empty()) {
                work.CreatePath = ApplyCreateExtension(work.CreatePath, CreateExtensionOf(work));
            }
        }

        void ClearInspect(struct Context::KeytoolWork &work) {
            work.HasInspect = false;
            work.InspectResult = {};
            work.InspectEntry = 0;
        }

        void ClearCreate(struct Context::KeytoolWork &work) {
            work.HasCreate = false;
            work.CreateResult = {};
            work.CreateEntry = 0;
        }

        void WipePasswords(struct Context::KeytoolWork &work) {
            WipeSecret(work.InspectPassword, sizeof work.InspectPassword);
            WipeSecret(work.CreatePassword, sizeof work.CreatePassword);
            WipeSecret(work.ConfirmPassword, sizeof work.ConfirmPassword);
            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.ShowInspectPassword = false;
            work.ShowCreatePassword = false;
            work.ShowConfirmPassword = false;
            work.ShowKeyPassword = false;
            work.SecretEpoch += 1;
        }

        void ResetForm(struct Context::KeytoolWork &work) {
            WipePasswords(work);
            work.Page = KeytoolPage::Inspect;
            work.InspectPath.clear();
            work.CreatePath.clear();
            work.KeyPasswordMatches = true;
            work.StoreType = KeystoreType::Pkcs12;
            work.CreateExtension = 0;
            work.CreateTemplate = 0;
            work.Algorithm = KeyAlgorithm::Rsa;
            work.KeySize = DefaultKeySize(KeyAlgorithm::Rsa);
            work.ValidityDays = DEFAULT_KEY_VALIDITY_DAYS;
            work.Alias[0] = '\0';
            work.CommonName[0] = '\0';
            work.OrganizationalUnit[0] = '\0';
            work.Organization[0] = '\0';
            work.Locality[0] = '\0';
            work.StateName[0] = '\0';
            work.Country[0] = '\0';
            ClearInspect(work);
            ClearCreate(work);
        }

        void LeaveKeytool(Context &context) {
            auto &work = context.KeytoolWork;
            RetireJobs(work);
            work.Busy = false;
            work.Task = KeytoolTask::None;
            ResetForm(work);
            context.UI.Workspace = WorkspacePage::Home;
        }

        void CopySecretBuffer(char *destination, const std::size_t destinationSize, const char *source) {
            if (destination == nullptr || destinationSize == 0) {
                return;
            }
            WipeSecret(destination, destinationSize);
            if (source == nullptr) {
                return;
            }
            const std::size_t length = std::min(std::strlen(source), destinationSize - 1);
            std::memcpy(destination, source, length);
            destination[length] = '\0';
        }

        float ComboTextWidth(const char *label) {
            const ImGuiStyle &style = ImGui::GetStyle();
            return ImGui::CalcTextSize(label).x + style.FramePadding.x + ImGui::GetFrameHeight() + Em(1.5F);
        }

        float KeySizeComboWidth() {
            return ComboTextWidth("4096-bit");
        }

        float TemplateComboWidth() {
            float width = 0.0F;
            for (const CreateTemplateOption &option: CREATE_TEMPLATES) {
                width = std::max(width, ComboTextWidth(Tr(option.Label)));
            }
            return width;
        }

        const KeySizeOption *SizesFor(const KeyAlgorithm algorithm, int &count) {
            switch (algorithm) {
                case KeyAlgorithm::Rsa:
                    count = IM_ARRAYSIZE(RSA_SIZES);
                    return RSA_SIZES;
                case KeyAlgorithm::Ec:
                    count = IM_ARRAYSIZE(EC_SIZES);
                    return EC_SIZES;
                case KeyAlgorithm::Dsa:
                    count = IM_ARRAYSIZE(DSA_SIZES);
                    return DSA_SIZES;
                case KeyAlgorithm::Ed25519:
                case KeyAlgorithm::Ed448:
                    break;
            }
            count = 0;
            return nullptr;
        }

        int FindSizeIndex(const KeyAlgorithm algorithm, const int keySize) {
            int count = 0;
            const KeySizeOption *sizes = SizesFor(algorithm, count);
            for (int index = 0; index < count; ++index) {
                if (sizes[index].Size == keySize) {
                    return index;
                }
            }
            return -1;
        }

        void SnapKeySize(struct Context::KeytoolWork &work) {
            if (!KeyAlgorithmUsesSize(work.Algorithm)) {
                work.KeySize = 0;
                return;
            }
            if (FindSizeIndex(work.Algorithm, work.KeySize) < 0) {
                work.KeySize = DefaultKeySize(work.Algorithm);
            }
        }

        int SizeIndex(const struct Context::KeytoolWork &work) {
            const int index = FindSizeIndex(work.Algorithm, work.KeySize);
            return index < 0 ? 0 : index;
        }

        CreateKeystoreRequest IdentityRequest(const struct Context::KeytoolWork &work) {
            CreateKeystoreRequest request;
            request.KeystorePath = work.CreatePath;
            request.StoreType = work.StoreType;
            request.Alias = work.Alias;
            request.Algorithm = work.Algorithm;
            request.KeySize = work.KeySize;
            request.ValidityDays = work.ValidityDays;
            request.CommonName = work.CommonName;
            request.OrganizationalUnit = work.OrganizationalUnit;
            request.Organization = work.Organization;
            request.Locality = work.Locality;
            request.State = work.StateName;
            request.Country = work.Country;
            return request;
        }

        CreateKeystoreRequest CreateRequest(const struct Context::KeytoolWork &work) {
            CreateKeystoreRequest request = IdentityRequest(work);
            request.StorePassword = work.CreatePassword;
            if (work.StoreType != KeystoreType::Pkcs12 && !work.KeyPasswordMatches) {
                request.KeyPassword = work.KeyPassword;
            }
            return request;
        }

        CreateCheck CheckCreate(const struct Context::KeytoolWork &work) {
            CreateKeystoreRequest request = CreateRequest(work);
            CreateCheck check;
            check.Refusal = ValidateCreateKeystore(request);
            if (check.Refusal == KeytoolRefusal::None && std::strcmp(work.CreatePassword, work.ConfirmPassword) != 0) {
                check.PasswordMismatch = true;
            }
            WipeSecret(request.StorePassword);
            WipeSecret(request.KeyPassword);
            return check;
        }

        bool CreateDirty(const struct Context::KeytoolWork &work) {
            return !work.CreatePath.empty() || work.Alias[0] != '\0' || work.CommonName[0] != '\0' || work.CreatePassword[0] != '\0' || work.Country[0] != '\0';
        }

        const char *RefusalText(const KeytoolRefusal refusal) {
            switch (refusal) {
                case KeytoolRefusal::MissingKeytool:
                    return Tr("keytool was not found in the JDK.");
                case KeytoolRefusal::EmptyPath:
                    return Tr("Choose a destination file.");
                case KeytoolRefusal::MissingFile:
                    return Tr("The keystore file could not be found.");
                case KeytoolRefusal::PathExists:
                    return Tr("A keystore already exists at this path.");
                case KeytoolRefusal::ParentMissing:
                    return Tr("The destination folder does not exist.");
                case KeytoolRefusal::EmptyAlias:
                    return Tr("Enter an alias.");
                case KeytoolRefusal::PasswordTooShort:
                case KeytoolRefusal::KeyPasswordTooShort:
                    return Tr("The password must be at least 6 characters.");
                case KeytoolRefusal::KeyPasswordDiffers:
                    return Tr("PKCS12 uses the keystore password for the key.");
                case KeytoolRefusal::InvalidValidity:
                    return Tr("Enter a validity of at least one day.");
                case KeytoolRefusal::InvalidKeySize:
                    return Tr("Choose a key size for this algorithm.");
                case KeytoolRefusal::InvalidCountry:
                    return Tr("Enter a two-letter country code.");
                case KeytoolRefusal::EmptyDistinguishedName:
                    return Tr("Enter a name for the certificate.");
                case KeytoolRefusal::PasswordFileFailed:
                    return Tr("The password could not be passed to keytool.");
                case KeytoolRefusal::None:
                    break;
            }
            return "";
        }

        const char *CreateFormMessage(const CreateCheck &check) {
            if (check.PasswordMismatch) {
                return Tr("The passwords do not match.");
            }
            if (check.Refusal == KeytoolRefusal::None) {
                return nullptr;
            }
            return RefusalText(check.Refusal);
        }

        const char *FailureMessage(const bool toolMissing, const KeytoolRefusal refusal, const std::string &output, const char *fallback) {
            if (toolMissing || refusal == KeytoolRefusal::MissingKeytool) {
                return Tr("keytool was not found in the JDK.");
            }
            if (refusal != KeytoolRefusal::None) {
                return RefusalText(refusal);
            }
            if (KeystorePasswordRejected(output)) {
                return Tr("The keystore password was incorrect.");
            }
            return fallback;
        }

        const char *DescribeFailure(const KeystoreDescription &description) {
            return FailureMessage(description.ToolMissing, description.Refusal, description.Output, Tr("The keystore could not be read."));
        }

        const char *CreateFailure(const CreateKeystoreResult &result) {
            return FailureMessage(result.ToolMissing, result.Refusal, result.Output, Tr("The keystore could not be created."));
        }

        void PollKeytool(Context &context) {
            PollRetired();
            auto &work = context.KeytoolWork;
            if (!work.Busy.load()) {
                return;
            }

            const bool inspectFinished = work.Task == KeytoolTask::Inspect && TakeFinished(work.InspectFuture, work.InspectResult, work.HasInspect);
            const bool createFinished = work.Task == KeytoolTask::Create && TakeFinished(work.CreateFuture, work.CreateResult, work.HasCreate);
            if (inspectFinished || createFinished) {
                if (inspectFinished) {
                    work.InspectEntry = 0;
                } else {
                    work.CreateEntry = 0;
                }
                work.Busy = false;
                work.Task = KeytoolTask::None;
                return;
            }

            const bool inspectPending = work.Task == KeytoolTask::Inspect && work.InspectFuture.valid();
            const bool createPending = work.Task == KeytoolTask::Create && work.CreateFuture.valid();
            if (!inspectPending && !createPending) {
                work.Busy = false;
                work.Task = KeytoolTask::None;
            }
        }

        void StartInspect(Context &context) {
            auto &work = context.KeytoolWork;
            if (work.Busy.load() || work.InspectPath.empty() || work.InspectPassword[0] == '\0') {
                return;
            }

            std::string storePassword(work.InspectPassword);
            SecretText secret{std::move(storePassword)};
            WipeSecret(storePassword);
            const JdkInfo jdk = context.Host.Jdk;
            const std::string keystore = work.InspectPath;
            const EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            ClearInspect(work);
            work.InspectFuture = std::async(std::launch::async, [secret = std::move(secret), jdk, keystore, toolEnv]() mutable {
                KeystoreDescription result = DescribeKeystore(jdk, keystore, std::move(secret.Value), toolEnv);
                WipeSecret(secret.Value);
                return result;
            });
            work.Task = KeytoolTask::Inspect;
            work.Busy = true;
        }

        void StartCreate(Context &context) {
            auto &work = context.KeytoolWork;
            if (work.Busy.load() || !CheckCreate(work).Ready()) {
                return;
            }

            CreateKeystoreRequest request = CreateRequest(work);
            request.Jdk = context.Host.Jdk;
            request.ToolEnv = context.Host.Sdk.ToolEnv;
            CreateLaunch launch{std::move(request)};
            WipeSecret(request.StorePassword);
            WipeSecret(request.KeyPassword);
            RetireJobs(work);
            ClearCreate(work);
            work.CreateFuture = std::async(std::launch::async, [launch = std::move(launch)]() mutable {
                return launch.Run();
            });
            work.Task = KeytoolTask::Create;
            work.Busy = true;
        }

        template<typename Signer>
        void UseInSigner(
            Context &context,
            Signer &signer,
            const WorkspacePage page,
            const std::string &path,
            const KeystoreDescription &description,
            const std::string &alias
        ) {
            auto &work = context.KeytoolWork;
            if (signer.Busy.load() || work.Busy.load() || path.empty() || alias.empty()) {
                return;
            }

            signer.KeystorePath = path;
            signer.KeystoreType = description.StoreType;
            if (signer.KeystoreType.empty() && work.Page == KeytoolPage::Create) {
                signer.KeystoreType = KeystoreTypeName(work.StoreType);
            }
            signer.Aliases.clear();
            for (const KeystoreEntry &entry: description.Entries) {
                if (entry.EntryType == "PrivateKeyEntry" && !entry.Alias.empty()) {
                    signer.Aliases.push_back(entry.Alias);
                }
            }
            signer.SelectedAlias = alias;
            signer.HasAliasResult = true;
            signer.AliasResult = {};
            signer.AliasResult.Status = KeytoolAliasStatus::Ok;
            signer.AliasResult.Started = true;
            signer.AliasListScheduled = false;
            signer.AliasRefreshPending = false;
            signer.HasSignResult = false;
            signer.SignResult = {};

            const char *storePassword = work.Page == KeytoolPage::Inspect ? work.InspectPassword : work.CreatePassword;
            CopySecretBuffer(signer.StorePassword, sizeof signer.StorePassword, storePassword);
            const bool distinctKey = work.Page == KeytoolPage::Create && work.StoreType != KeystoreType::Pkcs12 && !work.KeyPasswordMatches;
            signer.KeyPasswordMatches = !distinctKey;
            if (distinctKey) {
                CopySecretBuffer(signer.KeyPassword, sizeof signer.KeyPassword, work.KeyPassword);
            } else {
                WipeSecret(signer.KeyPassword, sizeof signer.KeyPassword);
                signer.ShowKeyPassword = false;
            }
            signer.ShowStorePassword = false;
            signer.SecretEpoch += 1;
            context.UI.Workspace = page;
        }

        const char *EntryTypeLabel(const std::string &type) {
            if (type == "PrivateKeyEntry") {
                return Tr("Private Key");
            }
            if (type == "trustedCertEntry") {
                return Tr("Trusted certificate");
            }
            if (type == "SecretKeyEntry") {
                return Tr("Secret key");
            }
            return type.c_str();
        }

        const char *EntryTypeAccent(const std::string &type) {
            if (type == "PrivateKeyEntry") {
                return Colors::ACCENT_INFO;
            }
            if (type == "trustedCertEntry") {
                return Colors::WARNING;
            }
            if (type == "SecretKeyEntry") {
                return Colors::POSITIVE;
            }
            return Colors::TEXT_MUTED;
        }

        void AddDetail(std::vector<SplitDetail> &details, const char *label, std::string value) {
            details.push_back({.Label = label, .Value = std::move(value)});
        }

        void AddDetailIf(std::vector<SplitDetail> &details, const char *label, const std::string &value) {
            if (!value.empty()) {
                AddDetail(details, label, value);
            }
        }

        void AppendFileDetails(std::vector<SplitDetail> &details, const std::string &path) {
            if (path.empty()) {
                return;
            }
            AddDetail(details, Tr("File Name"), FileNameOf(path));
            AddDetail(details, Tr("Full Path"), path);
        }

        void AppendCertificateReport(std::vector<SplitDetail> &details, std::vector<CopyableValue> &copies, const KeystoreEntry &entry) {
            const int count = static_cast<int>(entry.Certificates.size());
            for (int index = 0; index < count; ++index) {
                const KeystoreCertificate &certificate = entry.Certificates.at(static_cast<std::size_t>(index));
                std::string section;
                if (count > 1) {
                    section = TrFormat("Certificate {0}", std::to_string(index + 1));
                    details.push_back({.Label = section, .Kind = SplitDetailKind::Section});
                }
                AddDetailIf(details, Tr("Owner"), certificate.Owner);
                AddDetailIf(details, Tr("Issuer"), certificate.Issuer);
                AddDetailIf(details, Tr("Serial Number"), certificate.SerialNumber);
                AddDetail(details, Tr("Valid From"), certificate.ValidFrom);
                AddDetail(details, Tr("Valid Until"), certificate.ValidUntil);
                AddDetailIf(details, Tr("Signature"), certificate.SignatureAlgorithm);
                AddDetailIf(details, Tr("Public Key"), certificate.PublicKey);
                for (const KeystoreFingerprint &fingerprint: certificate.Fingerprints) {
                    copies.push_back({.Label = fingerprint.Algorithm, .Value = fingerprint.Value, .Section = section});
                }
            }
        }

        const KeystoreEntry *SelectedEntry(const KeystoreDescription &description, int &selected) {
            if (description.Entries.empty()) {
                return nullptr;
            }
            selected = std::clamp(selected, 0, static_cast<int>(description.Entries.size()) - 1);
            return &description.Entries.at(static_cast<std::size_t>(selected));
        }

        void DrawEntryPicker(const KeystoreDescription &description, int &selected) {
            if (description.Entries.size() < 2) {
                return;
            }
            selected = std::clamp(selected, 0, static_cast<int>(description.Entries.size()) - 1);
            std::vector<const char *> labels;
            labels.reserve(description.Entries.size());
            for (const KeystoreEntry &entry: description.Entries) {
                labels.push_back(entry.Alias.empty() ? UNAVAILABLE : entry.Alias.c_str());
            }
            const std::string &alias = description.Entries.at(static_cast<std::size_t>(selected)).Alias;
            const char *preview = alias.empty() ? Tr("Select an alias") : alias.c_str();
            FieldLabel(Tr("Alias"));
            RoundedCombo("##EntryAlias", preview, labels.data(), static_cast<int>(labels.size()), selected, -1.0F, Tr("Alias"));
            ImGui::Spacing();
        }

        float SignerButtonWidth(const char *label) {
            return ImGui::CalcTextSize(label).x + (ImGui::GetStyle().FramePadding.x * 2.0F);
        }

        void DrawKeystoreHeading(Context &context, const std::string &path, const KeystoreDescription &description, const KeystoreEntry *entry) {
            const bool handoff = entry != nullptr && entry->EntryType == "PrivateKeyEntry" && !entry->Alias.empty();

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", Tr("Keystore"));
            if (!handoff) {
                return;
            }

            const char *apkLabel = Tr("Use in APK Signer");
            const char *jarLabel = Tr("Use in Jar Signer");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float apkW = SignerButtonWidth(apkLabel);
            const float jarW = SignerButtonWidth(jarLabel);
            const float total = apkW + spacing + jarW;
            ImGui::SameLine();
            const float remaining = ImGui::GetContentRegionAvail().x;
            if (remaining > total) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - total);
            }
            const bool keytoolBusy = context.KeytoolWork.Busy.load();
            const std::string &alias = entry->Alias;
            if (PrimaryButton(StrConcat(apkLabel, "##UseInApkSigner").c_str(), !context.ApkSignerWork.Busy.load() && !keytoolBusy, ImVec2(apkW, 0.0F))) {
                UseInSigner(context, context.ApkSignerWork, WorkspacePage::ApkSigner, path, description, alias);
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(StrConcat(jarLabel, "##UseInJarSigner").c_str(), !context.JarSignerWork.Busy.load() && !keytoolBusy, ImVec2(jarW, 0.0F))) {
                UseInSigner(context, context.JarSignerWork, WorkspacePage::JarSigner, path, description, alias);
            }
        }

        void DrawDescription(Context &context, const std::string &path, const KeystoreDescription &description, int &selected) {
            const KeystoreEntry *entry = SelectedEntry(description, selected);
            DrawKeystoreHeading(context, path, description, entry);
            ImGui::Spacing();
            if (description.Entries.size() > 1) {
                DrawEntryPicker(description, selected);
                entry = SelectedEntry(description, selected);
            }

            std::vector<SplitDetail> details;
            std::vector<CopyableValue> copies;
            AppendFileDetails(details, path);
            AddDetail(details, Tr("Store Type"), description.StoreType);
            AddDetail(details, Tr("Provider"), description.Provider);
            const int count = static_cast<int>(description.Entries.size());
            AddDetail(details, Tr("Entries"), TrFormatN("{0} Entry", "{0} Entries", count, std::to_string(count)));
            if (entry != nullptr) {
                const std::string typeLabel = entry->EntryType.empty() ? std::string(UNAVAILABLE) : EntryTypeLabel(entry->EntryType);
                details.push_back({.Label = Tr("Type"), .Value = typeLabel, .Accent = EntryTypeAccent(entry->EntryType), .Kind = SplitDetailKind::Chip});
                AddDetailIf(details, Tr("Alias"), entry->Alias);
                AddDetail(details, Tr("Created"), entry->CreationDate);
                AppendCertificateReport(details, copies, *entry);
            }
            SplitDetailReport(details, copies);
            if (description.Entries.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("No entries were found in this keystore."));
            } else if (entry != nullptr && entry->Certificates.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("This entry has no certificate."));
            }
        }

        void DrawInspectReport(Context &context) {
            auto &work = context.KeytoolWork;
            const bool reading = work.Busy.load() && work.Task == KeytoolTask::Inspect;
            if (reading) {
                CenteredMessage(Tr("Reading Keystore..."), Tr("Keytool is listing the entries."));
                return;
            }
            if (!work.HasInspect) {
                CenteredMessage(Tr("No Keystore Inspected"), Tr("Choose a keystore and read its entries."));
                return;
            }
            if (work.InspectResult.Status != KeytoolStatus::Ok) {
                CenteredMessage(Tr("Something Went Wrong!"), DescribeFailure(work.InspectResult), Colors::NEGATIVE);
                return;
            }
            DrawDescription(context, work.InspectPath, work.InspectResult, work.InspectEntry);
        }

        std::string AlgorithmSummary(const struct Context::KeytoolWork &work) {
            std::string algorithm = KeyAlgorithmName(work.Algorithm);
            if (KeyAlgorithmUsesSize(work.Algorithm) && work.KeySize > 0) {
                algorithm += ' ';
                algorithm += std::to_string(work.KeySize);
                algorithm += "-bit";
            }
            return algorithm;
        }

        void DrawCreatePreview(const struct Context::KeytoolWork &work) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", Tr("New Keystore"));
            ImGui::Spacing();
            std::vector<SplitDetail> details;
            AppendFileDetails(details, work.CreatePath);
            AddDetail(details, Tr("Store Type"), KeystoreTypeName(work.StoreType));
            AddDetail(details, Tr("Alias"), work.Alias);
            AddDetail(details, Tr("Key Algorithm"), AlgorithmSummary(work));
            AddDetail(details, Tr("Validity"), TrFormat("{0} Days", std::to_string(work.ValidityDays)));
            AddDetail(details, Tr("Distinguished Name"), BuildDistinguishedName(IdentityRequest(work)));
            SplitDetailReport(details);
        }

        void DrawCreateReport(Context &context) {
            auto &work = context.KeytoolWork;
            if (work.Busy.load() && work.Task == KeytoolTask::Create) {
                CenteredMessage(Tr("Creating keystore..."), Tr("Keytool is generating the key pair."));
                return;
            }
            if (!work.HasCreate) {
                DrawCreatePreview(work);
                return;
            }
            if (work.CreateResult.Status != KeytoolStatus::Ok) {
                CenteredMessage(Tr("Something Went Wrong!"), CreateFailure(work.CreateResult), Colors::NEGATIVE);
                return;
            }
            if (work.CreateResult.Description.Status != KeytoolStatus::Ok) {
                CenteredMessage(
                    Tr("Something Went Wrong!"),
                    Tr("The keystore was created, but its entries could not be read."),
                    Colors::NEGATIVE
                );
                return;
            }
            DrawDescription(context, work.CreatePath, work.CreateResult.Description, work.CreateEntry);
        }

        std::string UpperExtension(std::string extension) {
            std::ranges::transform(extension, extension.begin(), [](const unsigned char character) {
                if (character >= 'a' && character <= 'z') {
                    return static_cast<char>(character - ('a' - 'A'));
                }
                return static_cast<char>(character);
            });
            return extension;
        }

        std::vector<std::string> KeystorePatterns() {
            std::vector<std::string> patterns;
            patterns.reserve((static_cast<std::size_t>(IM_ARRAYSIZE(KEYSTORE_SUFFIXES))) * 2);
            for (const char *suffix: KEYSTORE_SUFFIXES) {
                patterns.push_back(std::string("*") + suffix);
                patterns.push_back(std::string("*") + UpperExtension(suffix));
            }
            return patterns;
        }

        void DrawInspectBar(Context &context) {
            auto &work = context.KeytoolWork;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float eye = ImGui::GetFrameHeight();
            const char *idleLabel = Tr("Inspect");
            const char *busyLabel = Tr("Reading...");
            const float inspectW = BusyActionWidth(idleLabel, busyLabel);
            float passwordW = Em(22.0F);
            float pathW = avail - (passwordW + spacing + eye) - inspectW - (spacing * 2.0F);
            const float pathFloor = Em(16.0F);
            if (pathW < pathFloor) {
                passwordW = std::max(Em(12.0F), passwordW - (pathFloor - pathW));
                pathW = std::max(1.0F, avail - (passwordW + spacing + eye) - inspectW - (spacing * 2.0F));
            }
            const float secretW = passwordW + spacing + eye;

            {
                FormDisabled disabled(work.Busy.load());
                if (BrowseRow("##InspectPath", work.InspectPath, Tr("Choose a keystore file..."), "InspectPath", Tr("Browse..."), pathW, Tr("Keystore"))) {
                    const std::string start = DirectoryOf(work.InspectPath);
                    if (const auto picked = FileDialog::PickFile(Tr("Open Keystore"), Tr("Keystores"), KeystorePatterns(), start)) {
                        work.InspectPath = *picked;
                        ClearInspect(work);
                    }
                }
                ImGui::SameLine(0.0F, spacing);
                ImGui::PushID(static_cast<int>(work.SecretEpoch));
                const SecretEdit password = SecretField(
                    "##InspectPassword",
                    work.InspectPassword,
                    sizeof work.InspectPassword,
                    work.ShowInspectPassword,
                    Tr("Keystore Password"),
                    secretW
                );
                ImGui::PopID();
                if (password.Edited) {
                    ClearInspect(work);
                }
                if (password.Submitted) {
                    StartInspect(context);
                }
            }
            ImGui::SameLine(0.0F, spacing);
            const bool reading = work.Busy.load() && work.Task == KeytoolTask::Inspect;
            const bool enabled = !work.InspectPath.empty() && work.InspectPassword[0] != '\0';
            if (BusyButton("##InspectKeystore", idleLabel, busyLabel, reading, enabled, inspectW)) {
                StartInspect(context);
            }
        }

        void DrawCreateForm(Context &context) {
            auto &work = context.KeytoolWork;
            SnapKeySize(work);
            const auto clearResult = [&work] {
                if (!work.Busy.load()) {
                    ClearCreate(work);
                }
            };
            const auto touch = [&work, &clearResult] {
                clearResult();
                work.CreateTemplate = 0;
            };
            const auto drawHint = [&touch](const char *id, char *buffer, const std::size_t size, const char *hint, const float width, const char *tooltip = nullptr) {
                ImGui::SetNextItemWidth(width);
                if (ImGui::InputTextWithHint(id, hint, buffer, size)) {
                    touch();
                }
                HoverTooltip(tooltip != nullptr ? tooltip : hint);
            };

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const char *idleLabel = Tr("Create keystore");
            const char *busyLabel = Tr("Creating...");
            const float createW = BusyActionWidth(idleLabel, busyLabel);
            const float templateW = TemplateComboWidth();
            const float pathW = std::max(1.0F, avail - templateW - createW - (spacing * 2.0F));

            SecretEdit storePassword;
            SecretEdit confirmPassword;
            {
                FormDisabled disabled(work.Busy.load());
                const char *templateLabels[IM_ARRAYSIZE(CREATE_TEMPLATES)];
                for (int index = 0; index < IM_ARRAYSIZE(CREATE_TEMPLATES); ++index) {
                    templateLabels[index] = Tr(CREATE_TEMPLATES[index].Label);
                }
                int templateIndex = std::clamp(work.CreateTemplate, 0, IM_ARRAYSIZE(CREATE_TEMPLATES) - 1);
                if (RoundedCombo("##CreateTemplate", templateLabels[templateIndex], templateLabels, IM_ARRAYSIZE(CREATE_TEMPLATES), templateIndex, templateW, Tr("Template"))) {
                    ApplyCreateTemplate(work, templateIndex);
                }
                ImGui::SameLine(0.0F, spacing);
                if (BrowseRow("##CreatePath", work.CreatePath, Tr("Choose a destination file..."), "CreatePath", Tr("Save As..."), pathW, Tr("Destination"))) {
                    const char *extension = CreateExtensionOf(work);
                    const std::string upperExtension = UpperExtension(extension);
                    if (const auto picked = FileDialog::SaveFile(
                            Tr("Save Keystore"),
                            SuggestedKeystore(work),
                            Tr("Keystores"),
                            {std::string("*") + extension, std::string("*") + upperExtension}
                        )) {
                        AssignCreatePath(work, *picked);
                        clearResult();
                        if (work.CreateTemplate > 0) {
                            const int wanted = SuffixIndex(CREATE_TEMPLATES[work.CreateTemplate].Extension);
                            if (work.CreateExtension != wanted) {
                                work.CreateTemplate = 0;
                            }
                        }
                    }
                }
            }
            ImGui::SameLine(0.0F, spacing);
            const CreateCheck createCheck = CheckCreate(work);
            const bool creating = work.Busy.load() && work.Task == KeytoolTask::Create;
            if (BusyButton("##CreateKeystore", idleLabel, busyLabel, creating, createCheck.Ready(), createW)) {
                StartCreate(context);
            }

            ImGui::Spacing();
            {
                FormDisabled disabled(work.Busy.load());
                const bool usesSize = KeyAlgorithmUsesSize(work.Algorithm);
                const int gaps = usesSize ? 5 : 4;
                const float usable = std::max(1.0F, avail - (spacing * static_cast<float>(gaps)));
                float storeW = ComboTextWidth("PKCS12");
                float extensionW = ComboTextWidth(".keystore");
                float aliasW = Em(12.0F);
                float algW = ComboTextWidth("Ed25519");
                float sizeW = usesSize ? KeySizeComboWidth() : 0.0F;
                float validW = std::max(Em(8.0F), ImGui::CalcTextSize("10000").x + (ImGui::GetStyle().FramePadding.x * 2.0F));
                const float minimum = storeW + extensionW + aliasW + algW + sizeW + validW;
                const float extra = usable - minimum;
                if (extra > 0.0F) {
                    const float storeWeight = 1.15F;
                    const float extensionWeight = 1.25F;
                    const float aliasWeight = 1.6F;
                    const float algWeight = 1.25F;
                    const float sizeWeight = usesSize ? 1.35F : 0.0F;
                    const float validWeight = 1.0F;
                    const float weightSum = storeWeight + extensionWeight + aliasWeight + algWeight + sizeWeight + validWeight;
                    storeW += extra * (storeWeight / weightSum);
                    extensionW += extra * (extensionWeight / weightSum);
                    aliasW += extra * (aliasWeight / weightSum);
                    algW += extra * (algWeight / weightSum);
                    sizeW += extra * (sizeWeight / weightSum);
                    validW += extra * (validWeight / weightSum);
                } else if (extra < 0.0F) {
                    aliasW = std::max(Em(8.0F), aliasW + extra);
                }

                int storeType = static_cast<int>(work.StoreType);
                if (RoundedCombo("##StoreType", STORE_TYPE_LABELS[storeType], STORE_TYPE_LABELS, IM_ARRAYSIZE(STORE_TYPE_LABELS), storeType, storeW, Tr("Store Type"))) {
                    work.StoreType = static_cast<KeystoreType>(storeType);
                    if (work.StoreType == KeystoreType::Pkcs12) {
                        work.KeyPasswordMatches = true;
                        WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
                        work.ShowKeyPassword = false;
                        work.SecretEpoch += 1;
                    }
                    touch();
                }
                ImGui::SameLine(0.0F, spacing);
                int extension = std::clamp(work.CreateExtension, 0, IM_ARRAYSIZE(KEYSTORE_SUFFIXES) - 1);
                if (RoundedCombo("##CreateExtension", KEYSTORE_SUFFIXES[extension], KEYSTORE_SUFFIXES, IM_ARRAYSIZE(KEYSTORE_SUFFIXES), extension, extensionW, Tr("File Extension"))) {
                    work.CreateExtension = extension;
                    work.CreatePath = ApplyCreateExtension(work.CreatePath, CreateExtensionOf(work));
                    touch();
                }
                ImGui::SameLine(0.0F, spacing);
                drawHint("##Alias", work.Alias, sizeof work.Alias, Tr("Alias"), aliasW);
                ImGui::SameLine(0.0F, spacing);
                int algorithm = static_cast<int>(work.Algorithm);
                if (RoundedCombo("##Algorithm", ALGORITHM_LABELS[algorithm], ALGORITHM_LABELS, IM_ARRAYSIZE(ALGORITHM_LABELS), algorithm, algW, Tr("Key Algorithm"))) {
                    work.Algorithm = static_cast<KeyAlgorithm>(algorithm);
                    SnapKeySize(work);
                    touch();
                }
                if (usesSize) {
                    ImGui::SameLine(0.0F, spacing);
                    int count = 0;
                    const KeySizeOption *sizes = SizesFor(work.Algorithm, count);
                    int selected = SizeIndex(work);
                    const char *labels[IM_ARRAYSIZE(RSA_SIZES)] = {};
                    const char *preview = UNAVAILABLE;
                    if (count > 0 && sizes != nullptr) {
                        preview = sizes[selected].Label;
                        for (int index = 0; index < count; ++index) {
                            labels[index] = sizes[index].Label;
                        }
                    }
                    if (RoundedCombo("##KeySize", preview, labels, count, selected, sizeW, Tr("Key Size")) && sizes != nullptr) {
                        work.KeySize = sizes[selected].Size;
                        touch();
                    }
                }
                ImGui::SameLine(0.0F, spacing);
                ImGui::SetNextItemWidth(validW);
                if (ImGui::InputInt("##Validity", &work.ValidityDays, 0, 0)) {
                    work.ValidityDays = std::clamp(work.ValidityDays, 1, 365000);
                    touch();
                }
                HoverTooltip(Tr("Validity (days)"));

                if (work.StoreType != KeystoreType::Pkcs12) {
                    ImGui::Spacing();
                    if (SubtitledCheckbox(
                            "##KeyPasswordMatches",
                            &work.KeyPasswordMatches,
                            Tr("Key password matches keystore password"),
                            Tr("Turn this off if the key entry has a different password.")
                        )) {
                        if (work.KeyPasswordMatches) {
                            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
                            work.ShowKeyPassword = false;
                            work.SecretEpoch += 1;
                        }
                        clearResult();
                    }
                }
                const bool distinctKey = work.StoreType != KeystoreType::Pkcs12 && !work.KeyPasswordMatches;

                ImGui::Spacing();
                const int secretCount = distinctKey ? 3 : 2;
                const float secretW = std::max(1.0F, (avail - (spacing * static_cast<float>(secretCount - 1))) / static_cast<float>(secretCount));
                ImGui::PushID(static_cast<int>(work.SecretEpoch));
                storePassword = SecretField(
                    "##CreateStorePassword",
                    work.CreatePassword,
                    sizeof work.CreatePassword,
                    work.ShowCreatePassword,
                    Tr("Keystore Password"),
                    secretW
                );
                ImGui::PopID();
                if (storePassword.Edited) {
                    clearResult();
                }
                ImGui::SameLine(0.0F, spacing);
                ImGui::PushID(static_cast<int>(work.SecretEpoch) + 1);
                confirmPassword = SecretField(
                    "##ConfirmPassword",
                    work.ConfirmPassword,
                    sizeof work.ConfirmPassword,
                    work.ShowConfirmPassword,
                    Tr("Confirm Password"),
                    secretW
                );
                ImGui::PopID();
                if (confirmPassword.Edited) {
                    clearResult();
                }
                if (distinctKey) {
                    ImGui::SameLine(0.0F, spacing);
                    ImGui::PushID(static_cast<int>(work.SecretEpoch) + 2);
                    const SecretEdit keyPassword = SecretField(
                        "##CreateKeyPassword",
                        work.KeyPassword,
                        sizeof work.KeyPassword,
                        work.ShowKeyPassword,
                        Tr("Key Password"),
                        secretW
                    );
                    ImGui::PopID();
                    if (keyPassword.Edited) {
                        clearResult();
                    }
                }

                SectionLabel(Tr("Distinguished Name"));
                const float countryW = Em(8.0F);
                const float nameW = std::max(1.0F, (avail - countryW - (spacing * 5.0F)) / 5.0F);
                drawHint("##CommonName", work.CommonName, sizeof work.CommonName, Tr("Common Name"), nameW);
                ImGui::SameLine(0.0F, spacing);
                drawHint("##OrganizationalUnit", work.OrganizationalUnit, sizeof work.OrganizationalUnit, Tr("Organizational Unit"), nameW);
                ImGui::SameLine(0.0F, spacing);
                drawHint("##Organization", work.Organization, sizeof work.Organization, Tr("Organization"), nameW);
                ImGui::SameLine(0.0F, spacing);
                drawHint("##Locality", work.Locality, sizeof work.Locality, Tr("City"), nameW);
                ImGui::SameLine(0.0F, spacing);
                drawHint("##State", work.StateName, sizeof work.StateName, Tr("State"), nameW);
                ImGui::SameLine(0.0F, spacing);
                drawHint("##Country", work.Country, sizeof work.Country, "US", countryW, Tr("Country"));
            }
            if (storePassword.Submitted || confirmPassword.Submitted) {
                StartCreate(context);
            }
        }

        void DrawKeytoolStatus(Context &context) {
            auto &work = context.KeytoolWork;
            if (work.Page == KeytoolPage::Inspect) {
                const bool reading = work.Busy.load() && work.Task == KeytoolTask::Inspect;
                if (reading) {
                    StatusBar(StatusBarTone::Busy, Tr("Keytool is listing the entries."));
                    return;
                }
                if (!work.HasInspect) {
                    StatusBar(StatusBarTone::Idle, Tr("Choose a keystore and read its entries."));
                    return;
                }
                if (work.InspectResult.Status != KeytoolStatus::Ok) {
                    StatusBar(StatusBarTone::Error, DescribeFailure(work.InspectResult));
                    return;
                }
                StatusBar(StatusBarTone::Success, Tr("Keystore read."));
                return;
            }

            const bool creating = work.Busy.load() && work.Task == KeytoolTask::Create;
            if (creating) {
                StatusBar(StatusBarTone::Busy, Tr("Keytool is generating the key pair."));
                return;
            }
            if (work.HasCreate && work.CreateResult.Status == KeytoolStatus::Ok && work.CreateResult.Description.Status == KeytoolStatus::Ok) {
                StatusBar(StatusBarTone::Success, Tr("Keystore created."));
                return;
            }
            if (work.HasCreate && work.CreateResult.Status != KeytoolStatus::Ok) {
                StatusBar(StatusBarTone::Error, CreateFailure(work.CreateResult));
                return;
            }
            if (work.HasCreate) {
                StatusBar(StatusBarTone::Error, Tr("The keystore was created, but its entries could not be read."));
                return;
            }
            if (CreateDirty(work)) {
                const CreateCheck check = CheckCreate(work);
                if (!check.Ready()) {
                    if (const char *formMessage = CreateFormMessage(check)) {
                        StatusBar(StatusBarTone::Error, formMessage);
                        return;
                    }
                }
            }
            StatusBar(StatusBarTone::Idle, Tr("Ready to create a keystore."));
        }

        float ReportCardHeight() {
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            return std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - gap);
        }

        void DrawMissingJdk(Context &context) {
            if (NoticeCard(
                    NoticeCardTone::Error,
                    Tr("A JDK is required."),
                    Tr("Keytool creates and reads keystores with the JDK's keytool."),
                    Tr("Choose a JDK...")
                )) {
                context.UI.OpenPreferencesToJava = true;
                context.UI.ShowPreferences = true;
            }
        }

        void DrawKeytoolBody(Context &context) {
            auto &work = context.KeytoolWork;
            const char *pageLabels[] = {Tr("Inspect"), Tr("Create")};
            int page = static_cast<int>(work.Page);
            ImGui::BeginDisabled(work.Busy.load());
            if (CategoryChipRow(pageLabels, IM_ARRAYSIZE(pageLabels), page)) {
                work.Page = static_cast<KeytoolPage>(page);
            }
            ImGui::EndDisabled();
            ImGui::Spacing();

            if (work.Page == KeytoolPage::Inspect) {
                DrawInspectBar(context);
                ImGui::Spacing();
                DetailCard("##InspectDetail", ReportCardHeight(), [&context] {
                    DrawInspectReport(context);
                });
                DrawKeytoolStatus(context);
                return;
            }

            DrawCreateForm(context);
            ImGui::Spacing();
            DetailCard("##CreateDetail", ReportCardHeight(), [&context] {
                DrawCreateReport(context);
            });
            DrawKeytoolStatus(context);
        }
    }

    std::vector<std::string> KeystoreFilePatterns() {
        return KeystorePatterns();
    }

    void BuildKeytoolWindow(Context &context) {
        PollKeytool(context);
        if (context.UI.Workspace != WorkspacePage::Keytool) {
            return;
        }
        SetNativeWindowPage(context, Tr("Keytool"));

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            StyleVar sv;
            sv.Push(ImGuiStyleVar_WindowRounding, 0.0F);
            sv.Push(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::Begin("##Keytool", nullptr, WORKSPACE_WINDOW_FLAGS);
        }

        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##KeytoolBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        if (context.Host.Jdk.JavaHome.empty()) {
            DrawMissingJdk(context);
        } else {
            DrawKeytoolBody(context);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseKeytool(Context &context) {
        LeaveKeytool(context);
    }
}

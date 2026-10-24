//
// Created by AbdulMuaz Aqeel on 21/10/2026.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "jar_signer.h"
#include "../theme.h"
#include "../utilities.h"
#include "../widgets.h"
#include "../../core/file_dialog.h"
#include "../../core/i18n.h"
#include "../../core/jar_signer.h"
#include "../../core/keytool.h"
#include "../../core/paths.h"

namespace CoreDeck::Gui {
    namespace {
        struct StorePassword {
            std::string Value;

            explicit StorePassword(std::string value) : Value(std::move(value)) {
                Core::WipeSecret(value);
            }

            StorePassword(const StorePassword &) = delete;
            StorePassword &operator=(const StorePassword &) = delete;
            StorePassword &operator=(StorePassword &&) = delete;

            StorePassword(StorePassword &&other) noexcept : Value(std::move(other.Value)) {
                Core::WipeSecret(other.Value);
            }

            ~StorePassword() {
                Core::WipeSecret(Value);
            }
        };

        struct SignLaunch {
            Core::JarSignRequest Request;

            explicit SignLaunch(Core::JarSignRequest request) : Request(std::move(request)) {
                // Moved-from small strings can retain the secret in their inline buffer.
                Core::WipeSecret(request.StorePassword); // NOLINT(bugprone-use-after-move)
                Core::WipeSecret(request.KeyPassword); // NOLINT(bugprone-use-after-move)
            }

            SignLaunch(const SignLaunch &) = delete;
            SignLaunch &operator=(const SignLaunch &) = delete;
            SignLaunch &operator=(SignLaunch &&) = delete;

            SignLaunch(SignLaunch &&other) noexcept : Request(std::move(other.Request)) {
                Core::WipeSecret(other.Request.StorePassword);
                Core::WipeSecret(other.Request.KeyPassword);
            }

            ~SignLaunch() {
                Core::WipeSecret(Request.StorePassword);
                Core::WipeSecret(Request.KeyPassword);
            }

            Core::JarSignResult Run() {
                Core::JarSignResult result = Core::SignJar(std::move(Request));
                Core::WipeSecret(Request.StorePassword);
                Core::WipeSecret(Request.KeyPassword);
                return result;
            }
        };

        struct RetiredJobs {
            std::vector<std::future<Core::JarVerifyResult>> Verify;
            std::vector<std::future<Core::KeytoolAliasListResult>> Aliases;
            std::vector<std::future<Core::JarSignResult>> Sign;
        };

        RetiredJobs &Graveyard() {
            static RetiredJobs jobs;
            return jobs;
        }

        template<typename Result>
        bool TakeFinished(std::future<Result> &job, Result &result, const Result &failed) {
            if (!job.valid() || job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return false;
            }
            try {
                result = job.get();
            } catch (...) {
                result = failed;
            }
            return true;
        }

        void FinishTask(JarSignerWork &work) {
            work.Busy = false;
            work.Task = JarSignerTask::None;
        }

        void RetireJobs(JarSignerWork &work) {
            auto &grave = Graveyard();
            Retire(work.VerifyFuture, grave.Verify);
            Retire(work.AliasFuture, grave.Aliases);
            Retire(work.SignFuture, grave.Sign);
        }

        void PollRetired() {
            auto &grave = Graveyard();
            DropFinished(grave.Verify);
            DropFinished(grave.Aliases);
            DropFinished(grave.Sign);
        }

        bool JarsignerMissing(const std::string &path) {
            if (path.empty()) {
                return true;
            }
            std::error_code error;
            return !std::filesystem::is_regular_file(path, error);
        }

        void WipePasswordBuffers(JarSignerWork &work) {
            Core::WipeSecret(work.StorePassword, sizeof work.StorePassword);
            Core::WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.SecretEpoch += 1;
        }

        void ClearSignResult(JarSignerWork &work) {
            work.HasSignResult = false;
            work.SignResult = {};
        }

        void ResetForm(JarSignerWork &work) {
            WipePasswordBuffers(work);
            work.InputPath.clear();
            work.KeystorePath.clear();
            work.KeystoreType.clear();
            work.OutputPath.clear();
            work.OutputCustomized = false;
            work.KeyPasswordMatches = true;
            work.ShowStorePassword = false;
            work.ShowKeyPassword = false;
            work.HasVerify = false;
            work.Verify = {};
            work.HasAliasResult = false;
            work.AliasResult = {};
            work.Aliases.clear();
            work.SelectedAlias.clear();
            work.AliasListScheduled = false;
            ClearSignResult(work);
        }

        void LeaveJarSigner(Context &context) {
            auto &work = context.JarSignerWork;
            RetireJobs(work);
            work.Busy = false;
            work.Task = JarSignerTask::None;
            work.AliasRefreshPending = false;
            ResetForm(work);
            context.UI.Workspace = WorkspacePage::Home;
        }

        const char *RefusalText(const Core::JarSignRefusal refusal) {
            switch (refusal) {
                case Core::JarSignRefusal::MissingJarsigner:
                    return Core::Tr("jarsigner was not found in the JDK.");
                case Core::JarSignRefusal::MissingJdk:
                    return Core::Tr("A JDK is required.");
                case Core::JarSignRefusal::MissingOutput:
                    return Core::Tr("Choose an output path for the signed archive.");
                case Core::JarSignRefusal::OutputSameAsInput:
                    return Core::Tr("The output path is the same as the archive. The original archive is left unchanged.");
                case Core::JarSignRefusal::PasswordFileFailed:
                    return Core::Tr("The password could not be passed to jarsigner.");
                case Core::JarSignRefusal::None:
                    break;
            }
            return "";
        }

        Core::JarSignRefusal SignRefusal(const Context &context, const JarSignerWork &work, const std::string &jarsigner) {
            if (JarsignerMissing(jarsigner)) {
                return Core::JarSignRefusal::MissingJarsigner;
            }
            if (context.Host.Jdk.JavaHome.empty()) {
                return Core::JarSignRefusal::MissingJdk;
            }
            if (work.OutputPath.empty()) {
                return Core::JarSignRefusal::MissingOutput;
            }
            if (Paths::SameFilePath(work.InputPath, work.OutputPath)) {
                return Core::JarSignRefusal::OutputSameAsInput;
            }
            return Core::JarSignRefusal::None;
        }

        bool ShowLiveRefusal(const Core::JarSignRefusal refusal, const JarSignerWork &work) {
            switch (refusal) {
                case Core::JarSignRefusal::None:
                case Core::JarSignRefusal::MissingJarsigner:
                case Core::JarSignRefusal::PasswordFileFailed:
                    return false;
                case Core::JarSignRefusal::MissingOutput:
                    return !work.InputPath.empty();
                case Core::JarSignRefusal::MissingJdk:
                case Core::JarSignRefusal::OutputSameAsInput:
                    return true;
            }
            return false;
        }

        bool CanSign(const Context &context, const JarSignerWork &work, const std::string &jarsigner) {
            if (work.Busy.load() || work.InputPath.empty() || work.KeystorePath.empty() || work.SelectedAlias.empty()) {
                return false;
            }
            if (work.StorePassword[0] == '\0') {
                return false;
            }
            if (!work.KeyPasswordMatches && work.KeyPassword[0] == '\0') {
                return false;
            }
            return SignRefusal(context, work, jarsigner) == Core::JarSignRefusal::None;
        }

        void StartAliasList(Context &context);

        void ApplyVerify(JarSignerWork &work, Core::JarVerifyResult result) {
            work.Verify = std::move(result);
            work.HasVerify = true;
        }

        void ApplyAlias(JarSignerWork &work, Core::KeytoolAliasListResult result) {
            work.AliasResult = std::move(result);
            work.HasAliasResult = true;
            work.Aliases.clear();
            if (work.AliasResult.Status != Core::KeytoolAliasStatus::Ok) {
                work.SelectedAlias.clear();
                return;
            }
            work.Aliases = work.AliasResult.Aliases;
            if (work.Aliases.size() == 1) {
                work.SelectedAlias = work.Aliases.front();
            } else if (std::ranges::find(work.Aliases, work.SelectedAlias) == work.Aliases.end()) {
                work.SelectedAlias.clear();
            }
        }

        void ApplySign(JarSignerWork &work, Core::JarSignResult result) {
            work.SignResult = std::move(result);
            work.HasSignResult = true;
            WipePasswordBuffers(work);
        }

        void PollJarSigner(Context &context) {
            PollRetired();
            auto &work = context.JarSignerWork;
            if (work.Busy.load()) {
                Core::JarVerifyResult verify;
                Core::KeytoolAliasListResult aliases;
                Core::JarSignResult sign;
                Core::JarSignResult signFailed;
                signFailed.Status = Core::JarSignStatus::SignFailed;
                if (TakeFinished(work.VerifyFuture, verify, Core::JarVerifyResult{})) {
                    FinishTask(work);
                    ApplyVerify(work, std::move(verify));
                } else if (TakeFinished(work.AliasFuture, aliases, Core::KeytoolAliasListResult{})) {
                    FinishTask(work);
                    ApplyAlias(work, std::move(aliases));
                } else if (TakeFinished(work.SignFuture, sign, signFailed)) {
                    FinishTask(work);
                    ApplySign(work, std::move(sign));
                } else if (!work.VerifyFuture.valid() && !work.AliasFuture.valid() && !work.SignFuture.valid()) {
                    FinishTask(work);
                }
            }

            if (work.AliasListScheduled && std::chrono::steady_clock::now() >= work.AliasListDue) {
                work.AliasListScheduled = false;
                StartAliasList(context);
            }
            if (!work.Busy.load() && work.AliasRefreshPending) {
                StartAliasList(context);
            }
        }

        void StartVerify(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            if (work.Busy.load() || work.InputPath.empty() || JarsignerMissing(jarsigner)) {
                return;
            }
            const std::string archive = work.InputPath;
            const Core::EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.HasVerify = false;
            work.Verify = {};
            ClearSignResult(work);
            work.VerifyFuture = std::async(std::launch::async, [jarsigner, archive, toolEnv] {
                return Core::VerifyJar(jarsigner, archive, toolEnv);
            });
            work.Task = JarSignerTask::Verify;
            work.Busy = true;
        }

        void StartAliasList(Context &context) {
            auto &work = context.JarSignerWork;
            if (work.Busy.load()) {
                work.AliasRefreshPending = true;
                return;
            }
            work.AliasRefreshPending = false;
            work.AliasListScheduled = false;
            work.HasAliasResult = false;
            work.AliasResult = {};
            work.Aliases.clear();
            if (work.KeystorePath.empty() || work.StorePassword[0] == '\0') {
                work.SelectedAlias.clear();
                return;
            }

            std::string storePassword(work.StorePassword);
            StorePassword secret{std::move(storePassword)};
            Core::WipeSecret(storePassword);
            const Core::JdkInfo jdk = context.Host.Jdk;
            const std::string keystore = work.KeystorePath;
            const Core::EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.AliasFuture = std::async(
                std::launch::async,
                [secret = std::move(secret), jdk, keystore, toolEnv]() mutable {
                    Core::KeytoolAliasListResult result = Core::ListKeystoreAliases(jdk, keystore, std::move(secret.Value), toolEnv);
                    Core::WipeSecret(secret.Value);
                    return result;
                }
            );
            work.Task = JarSignerTask::Aliases;
            work.Busy = true;
        }

        void StartSign(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            if (work.Busy.load() || !CanSign(context, work, jarsigner)) {
                return;
            }

            std::string storePassword(work.StorePassword);
            std::string keyPassword = work.KeyPasswordMatches ? storePassword : std::string(work.KeyPassword);
            Core::JarSignRequest request;
            request.InputPath = work.InputPath;
            request.OutputPath = work.OutputPath;
            request.KeystorePath = work.KeystorePath;
            request.KeystoreType = work.KeystoreType;
            request.KeyAlias = work.SelectedAlias;
            request.Jdk = context.Host.Jdk;
            request.ToolEnv = context.Host.Sdk.ToolEnv;
            request.StorePassword = std::move(storePassword);
            Core::WipeSecret(storePassword);
            request.KeyPassword = std::move(keyPassword);
            Core::WipeSecret(keyPassword);

            SignLaunch launch{std::move(request)};
            Core::WipeSecret(request.StorePassword); // NOLINT(bugprone-use-after-move)
            Core::WipeSecret(request.KeyPassword); // NOLINT(bugprone-use-after-move)

            RetireJobs(work);
            ClearSignResult(work);
            work.SignFuture = std::async(std::launch::async, [launch = std::move(launch)]() mutable {
                return launch.Run();
            });
            work.Task = JarSignerTask::Sign;
            work.Busy = true;
        }

        std::array<const char *, 6> ReportColumns() {
            return {
                Core::Tr("File Name"),
                Core::Tr("Full Path"),
                Core::Tr("Signed By"),
                Core::Tr("Digest Algorithm"),
                Core::Tr("Signature Algorithm"),
                Core::Tr("Timestamp"),
            };
        }

        void VerifyDetails(const Core::JarVerifyResult &result) {
            if (!result.Started || result.ToolMissing) {
                return;
            }
            const auto columns = ReportColumns();
            LabeledValue(Core::Tr("Signed By"), result.Report.Signer, columns);
            ImGui::Spacing();
            LabeledValue(Core::Tr("Digest Algorithm"), result.Report.DigestAlgorithm, columns);
            ImGui::Spacing();
            LabeledValue(Core::Tr("Signature Algorithm"), result.Report.SignatureAlgorithm, columns);
            ImGui::Spacing();
            LabeledValue(Core::Tr("Timestamp"), result.Report.Timestamp, columns);
        }

        void SignatureReport(const char *title, const std::string &path, const Core::JarVerifyResult &verify) {
            const bool hasStatus = verify.Started && !verify.ToolMissing;
            ReportHeading(title, hasStatus ? &verify.Report.Verified : nullptr);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            FileRows(path, columns);
            ImGui::Spacing();
            VerifyDetails(verify);
        }

        void AliasCombo(JarSignerWork &work, const float width) {
            const bool reading = work.Busy.load() && work.Task == JarSignerTask::Aliases;
            const char *preview = Core::Tr("Select an Alias");
            if (reading) {
                preview = Core::Tr("Reading keystore aliases...");
            } else if (!work.SelectedAlias.empty()) {
                preview = work.SelectedAlias.c_str();
            }
            std::vector<const char *> labels;
            labels.reserve(work.Aliases.size());
            int selected = -1;
            for (int index = 0; std::cmp_less(index, work.Aliases.size()); ++index) {
                const std::string &alias = work.Aliases.at(static_cast<std::size_t>(index));
                labels.push_back(alias.c_str());
                if (alias == work.SelectedAlias) {
                    selected = index;
                }
            }
            ImGui::BeginDisabled(work.Aliases.empty());
            if (RoundedCombo("##Alias", preview, labels.data(), static_cast<int>(labels.size()), selected, width, Core::Tr("Alias")) && selected >= 0 && std::cmp_less(selected, work.Aliases.size())) {
                const std::string &alias = work.Aliases.at(static_cast<std::size_t>(selected));
                if (work.SelectedAlias != alias) {
                    ClearSignResult(work);
                }
                work.SelectedAlias = alias;
            }

            ImGui::EndDisabled();
        }

        const char *AliasNotice(const Context &context, StatusBarTone &tone) {
            const auto &work = context.JarSignerWork;
            const bool reading = work.Busy.load() && work.Task == JarSignerTask::Aliases;
            if (reading || !work.HasAliasResult) {
                return nullptr;
            }
            if (work.AliasResult.ToolMissing) {
                if (context.Host.Jdk.JavaHome.empty()) {
                    return nullptr;
                }
                tone = StatusBarTone::Error;
                return Core::Tr("keytool was not found in the JDK.");
            }
            if (work.AliasResult.Status != Core::KeytoolAliasStatus::Ok) {
                tone = StatusBarTone::Error;
                return Core::Tr("The keystore aliases could not be read.");
            }
            if (work.Aliases.empty()) {
                tone = StatusBarTone::Warning;
                return Core::Tr("No private-key aliases were found in this keystore.");
            }
            return nullptr;
        }

        void JarSignerOptions(JarSignerWork &work) {
            if (SubtitledCheckbox(
                    "##KeyPasswordMatches",
                    &work.KeyPasswordMatches,
                    Core::Tr("Key password matches keystore password"),
                    Core::Tr("Turn this off if the key entry has a different password.")
                ) &&
                work.KeyPasswordMatches) {
                Core::WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
                work.ShowKeyPassword = false;
                work.SecretEpoch += 1;
            }

            ImGui::Spacing();
            if (BrowseRow("##OutputPathField", work.OutputPath, Core::Tr("No output path"), "OutputPath", Core::Tr("Save As..."), -1.0F, Core::Tr("Signed archive"))) {
                const std::string suggested = work.OutputPath.empty() ? Core::SuggestedSignedJarPath(work.InputPath) : work.OutputPath;
                if (const auto picked = Core::FileDialog::SaveFile(
                        Core::Tr("Save signed archive"),
                        suggested,
                        Core::Tr("Java archives"),
                        Core::JarArchivePatterns()
                    )) {
                    work.OutputPath = *picked;
                    work.OutputCustomized = true;
                    ClearSignResult(work);
                }
            }
        }

        void SignerInputs(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const char *idleLabel = Core::Tr("Sign");
            const char *busyLabel = Core::Tr("Signing...");
            const float signW = BusyActionWidth(idleLabel, busyLabel);
            const float pathW = std::max(1.0F, (avail - signW - (spacing * 2.0F)) * 0.5F);

            bool refreshAliases = false;
            SecretEdit storePassword;
            {
                const FormDisabled disabled(work.Busy.load());
                if (BrowseRow("##ArchivePathField", work.InputPath, Core::Tr("Choose an archive..."), "ArchivePath", Core::Tr("Browse..."), pathW, Core::Tr("Archive"))) {
                    const std::string start = DirectoryOf(work.InputPath);
                    if (const auto picked = Core::FileDialog::PickFile(Core::Tr("Open Archive"), Core::Tr("Java archives"), Core::JarArchivePatterns(), start)) {
                        work.InputPath = *picked;
                        if (!work.OutputCustomized) {
                            work.OutputPath = Core::SuggestedSignedJarPath(work.InputPath);
                        }
                        work.HasVerify = false;
                        work.Verify = {};
                        ClearSignResult(work);
                        StartVerify(context, jarsigner);
                    }
                }
                ImGui::SameLine(0.0F, spacing);
                if (BrowseRow(
                        "##KeystorePathField",
                        work.KeystorePath,
                        Core::Tr("Choose a keystore file..."),
                        "KeystorePath",
                        Core::Tr("Browse..."),
                        pathW,
                        Core::Tr("Keystore")
                    )) {
                    const std::string start = work.KeystorePath.empty() ? DirectoryOf(work.InputPath) : DirectoryOf(work.KeystorePath);
                    if (const auto picked = Core::FileDialog::PickFile(Core::Tr("Open Keystore"), Core::Tr("Keystores"), Core::KeystoreFilePatterns(), start)) {
                        work.KeystorePath = *picked;
                        work.KeystoreType = Core::DetectKeystoreType(*picked);
                        ClearSignResult(work);
                        refreshAliases = true;
                    }
                }
            }
            ImGui::SameLine(0.0F, spacing);
            const bool signing = work.Busy.load() && work.Task == JarSignerTask::Sign;
            if (BusyButton("##SignJar", idleLabel, busyLabel, signing, CanSign(context, work, jarsigner), signW)) {
                StartSign(context, jarsigner);
            }

            ImGui::Spacing();
            {
                const FormDisabled disabled(work.Busy.load());
                const float eye = ImGui::GetFrameHeight();
                float secretW = Em(22.0F) + spacing + eye;
                float aliasW = std::max(1.0F, avail - secretW - spacing);
                if (!work.KeyPasswordMatches) {
                    const float keyFloor = Em(16.0F) + spacing + eye;
                    aliasW = std::max(Em(12.0F), (avail - (spacing * 2.0F)) / 3.0F);
                    secretW = std::max(keyFloor, (avail - aliasW - (spacing * 2.0F)) * 0.5F);
                    aliasW = std::max(1.0F, avail - (secretW * 2.0F) - (spacing * 2.0F));
                }
                ImGui::PushID(static_cast<int>(work.SecretEpoch));
                storePassword = SecretField(
                    "##StorePassword",
                    work.StorePassword,
                    sizeof work.StorePassword,
                    work.ShowStorePassword,
                    Core::Tr("Keystore Password"),
                    secretW
                );
                ImGui::PopID();
                if (storePassword.Submitted) {
                    work.AliasListScheduled = false;
                    refreshAliases = true;
                } else if (storePassword.Edited) {
                    work.AliasListDue = std::chrono::steady_clock::now() + std::chrono::milliseconds(1400);
                    work.AliasListScheduled = true;
                }
                ImGui::SameLine(0.0F, spacing);
                AliasCombo(work, aliasW);
                if (!work.KeyPasswordMatches) {
                    ImGui::SameLine(0.0F, spacing);
                    ImGui::PushID(static_cast<int>(work.SecretEpoch) + 1);
                    SecretField(
                        "##KeyPassword",
                        work.KeyPassword,
                        sizeof work.KeyPassword,
                        work.ShowKeyPassword,
                        Core::Tr("Key Password"),
                        secretW
                    );
                    ImGui::PopID();
                }

                ImGui::Spacing();
                JarSignerOptions(work);
            }
            if (refreshAliases) {
                StartAliasList(context);
            }
        }

        const char *SignFailure(const Core::JarSignResult &result) {
            switch (result.Status) {
                case Core::JarSignStatus::Refused:
                    return RefusalText(result.Refusal);
                case Core::JarSignStatus::SignFailed:
                    return Core::Tr("Signing failed.");
                case Core::JarSignStatus::Signed:
                    break;
            }
            return Core::Tr("Signing failed.");
        }

        void SignerStatus(const Context &context, const std::string &jarsigner) {
            const auto &work = context.JarSignerWork;
            const bool busy = work.Busy.load();
            if (busy && work.Task == JarSignerTask::Sign) {
                StatusBar(StatusBarTone::Busy, Core::Tr("Signing the archive."));
                return;
            }
            if (busy && work.Task == JarSignerTask::Verify) {
                StatusBar(StatusBarTone::Busy, Core::Tr("Checking the signature."));
                return;
            }
            if (busy && work.Task == JarSignerTask::Aliases) {
                StatusBar(StatusBarTone::Busy, Core::Tr("Reading the keystore aliases."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != Core::JarSignStatus::Signed) {
                StatusBar(StatusBarTone::Error, SignFailure(work.SignResult));
                return;
            }
            if (work.HasSignResult) {
                if (!work.SignResult.Verify.Started || work.SignResult.Verify.ToolMissing) {
                    StatusBar(StatusBarTone::Error, Core::Tr("The signature could not be checked."));
                    return;
                }
                StatusBar(StatusBarTone::Success, Core::Tr("Archive signed."));
                return;
            }
            if (!work.InputPath.empty() && work.HasVerify && (!work.Verify.Started || work.Verify.ToolMissing)) {
                StatusBar(StatusBarTone::Error, Core::Tr("The signature could not be checked."));
                return;
            }
            StatusBarTone aliasTone = StatusBarTone::Error;
            if (const char *aliasMessage = AliasNotice(context, aliasTone)) {
                StatusBar(aliasTone, aliasMessage);
                return;
            }
            const Core::JarSignRefusal refusal = SignRefusal(context, work, jarsigner);
            if (ShowLiveRefusal(refusal, work)) {
                StatusBar(StatusBarTone::Error, RefusalText(refusal));
                return;
            }
            if (work.InputPath.empty()) {
                StatusBar(StatusBarTone::Idle, Core::Tr("Choose an archive to see its signature."));
                return;
            }
            if (work.HasVerify && work.Verify.Started && !work.Verify.ToolMissing && work.Verify.Report.Verified) {
                StatusBar(
                    StatusBarTone::Warning,
                    Core::Tr(
                        "This archive is already signed. The new file carries a different signature, "
                        "and the original archive is left unchanged."
                    )
                );
                return;
            }
            StatusBar(StatusBarTone::Idle, Core::Tr("Ready to sign an archive."));
        }

        float ReportCardHeight() {
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float stack = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - (gap * 2.0F));
            return std::max(1.0F, stack * 0.5F);
        }

        void InputDetail(const JarSignerWork &work) {
            if (work.InputPath.empty()) {
                CenteredMessage(Core::Tr("No Archive Selected"), Core::Tr("Choose an archive to see its signature."));
                return;
            }
            if (work.Busy.load() && work.Task == JarSignerTask::Verify) {
                CenteredMessage(Core::Tr("Checking the signature..."), nullptr);
                return;
            }
            if (!work.HasVerify || !work.Verify.Started || work.Verify.ToolMissing) {
                CenteredMessage(Core::Tr("Something Went Wrong!"), Core::Tr("The signature could not be checked."), Colors::NEGATIVE);
                return;
            }
            SignatureReport(Core::Tr("Archive"), work.InputPath, work.Verify);
        }

        void OutputDetail(const JarSignerWork &work) {
            const bool signing = work.Busy.load() && work.Task == JarSignerTask::Sign;
            if (signing) {
                CenteredMessage(Core::Tr("Signing..."), nullptr);
                return;
            }
            if (!work.HasSignResult && work.OutputPath.empty()) {
                CenteredMessage(Core::Tr("No Signed Archive"), Core::Tr("Sign an archive to see the result here."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != Core::JarSignStatus::Signed) {
                CenteredMessage(Core::Tr("Something Went Wrong!"), SignFailure(work.SignResult), Colors::NEGATIVE);
                return;
            }
            if (!work.HasSignResult) {
                ReportHeading(Core::Tr("Signed Archive"));
                ImGui::Spacing();
                const auto columns = ReportColumns();
                FileRows(work.OutputPath, columns);
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Core::Tr("Sign the archive to see its signature here."));
                return;
            }

            SignatureReport(Core::Tr("Signed Archive"), work.OutputPath, work.SignResult.Verify);
        }

        void SignerForm(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            SignerInputs(context, jarsigner);
            ImGui::Spacing();
            const float cardH = ReportCardHeight();
            DetailCard("##InputDetail", cardH, [&work] { InputDetail(work); });
            DetailCard("##OutputDetail", cardH, [&work] { OutputDetail(work); });
            SignerStatus(context, jarsigner);
        }

        void MissingJarsigner(Context &context) {
            const bool jdkMissing = context.Host.Jdk.JavaHome.empty();
            if (NoticeCard(
                    NoticeCardTone::Error,
                    jdkMissing ? Core::Tr("A JDK is required.") : Core::Tr("jarsigner was not found in the JDK."),
                    Core::Tr("Java Jar Signer signs and verifies archives with the JDK's jarsigner."),
                    Core::Tr("Choose a JDK...")
                )) {
                context.UI.OpenPreferencesToJava = true;
                context.UI.ShowPreferences = true;
            }
        }
    }

    void BuildJarSignerWindow(Context &context) {
        PollJarSigner(context);
        if (context.UI.Workspace != WorkspacePage::JarSigner) {
            return;
        }
        SetNativeWindowPage(context, Core::Tr("Java Jar Signer"));

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            StyleVar sv;
            sv.Push(ImGuiStyleVar_WindowRounding, 0.0F);
            sv.Push(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::Begin("##JarSigner", nullptr, WORKSPACE_WINDOW_FLAGS);
        }

        const std::string jarsigner = Core::JarsignerPath(context.Host.Jdk);
        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##JarSignerBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        if (context.Host.Jdk.JavaHome.empty() || JarsignerMissing(jarsigner)) {
            MissingJarsigner(context);
        } else {
            SignerForm(context, jarsigner);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseJarSigner(Context &context) {
        LeaveJarSigner(context);
    }
}

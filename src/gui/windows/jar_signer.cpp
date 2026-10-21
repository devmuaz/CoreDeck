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

namespace CoreDeck {
    namespace {
        struct StorePassword {
            std::string Value;

            explicit StorePassword(std::string value) : Value(std::move(value)) {
                WipeSecret(value);
            }

            StorePassword(const StorePassword &) = delete;
            StorePassword &operator=(const StorePassword &) = delete;
            StorePassword &operator=(StorePassword &&) = delete;

            StorePassword(StorePassword &&other) noexcept : Value(std::move(other.Value)) {
                WipeSecret(other.Value);
            }

            ~StorePassword() {
                WipeSecret(Value);
            }
        };

        struct SignLaunch {
            JarSignRequest Request;

            explicit SignLaunch(JarSignRequest request) : Request(std::move(request)) {
                WipeSecret(request.StorePassword);
                WipeSecret(request.KeyPassword);
            }

            SignLaunch(const SignLaunch &) = delete;
            SignLaunch &operator=(const SignLaunch &) = delete;
            SignLaunch &operator=(SignLaunch &&) = delete;

            SignLaunch(SignLaunch &&other) noexcept : Request(std::move(other.Request)) {
                WipeSecret(other.Request.StorePassword);
                WipeSecret(other.Request.KeyPassword);
            }

            ~SignLaunch() {
                WipeSecret(Request.StorePassword);
                WipeSecret(Request.KeyPassword);
            }

            JarSignResult Run() {
                JarSignResult result = SignJar(std::move(Request));
                WipeSecret(Request.StorePassword);
                WipeSecret(Request.KeyPassword);
                return result;
            }
        };

        struct RetiredJobs {
            std::vector<std::future<JarVerifyResult>> Verify;
            std::vector<std::future<KeytoolAliasListResult>> Aliases;
            std::vector<std::future<JarSignResult>> Sign;
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
            WipeSecret(work.StorePassword, sizeof work.StorePassword);
            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
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

        const char *RefusalText(const JarSignRefusal refusal) {
            switch (refusal) {
                case JarSignRefusal::MissingJarsigner:
                    return Tr("jarsigner was not found in the JDK.");
                case JarSignRefusal::MissingJdk:
                    return Tr("A JDK is required.");
                case JarSignRefusal::MissingOutput:
                    return Tr("Choose an output path for the signed archive.");
                case JarSignRefusal::OutputSameAsInput:
                    return Tr("The output path is the same as the archive. The original archive is left unchanged.");
                case JarSignRefusal::PasswordFileFailed:
                    return Tr("The password could not be passed to jarsigner.");
                case JarSignRefusal::None:
                    break;
            }
            return "";
        }

        JarSignRefusal SignRefusal(const Context &context, const JarSignerWork &work, const std::string &jarsigner) {
            if (JarsignerMissing(jarsigner)) {
                return JarSignRefusal::MissingJarsigner;
            }
            if (context.Host.Jdk.JavaHome.empty()) {
                return JarSignRefusal::MissingJdk;
            }
            if (work.OutputPath.empty()) {
                return JarSignRefusal::MissingOutput;
            }
            if (Paths::SameFilePath(work.InputPath, work.OutputPath)) {
                return JarSignRefusal::OutputSameAsInput;
            }
            return JarSignRefusal::None;
        }

        bool ShowLiveRefusal(const JarSignRefusal refusal, const JarSignerWork &work) {
            switch (refusal) {
                case JarSignRefusal::None:
                case JarSignRefusal::MissingJarsigner:
                case JarSignRefusal::PasswordFileFailed:
                    return false;
                case JarSignRefusal::MissingOutput:
                    return !work.InputPath.empty();
                case JarSignRefusal::MissingJdk:
                case JarSignRefusal::OutputSameAsInput:
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
            return SignRefusal(context, work, jarsigner) == JarSignRefusal::None;
        }

        void StartAliasList(Context &context);

        void ApplyVerify(JarSignerWork &work, JarVerifyResult result) {
            work.Verify = std::move(result);
            work.HasVerify = true;
        }

        void ApplyAlias(JarSignerWork &work, KeytoolAliasListResult result) {
            work.AliasResult = std::move(result);
            work.HasAliasResult = true;
            work.Aliases.clear();
            if (work.AliasResult.Status != KeytoolAliasStatus::Ok) {
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

        void ApplySign(JarSignerWork &work, JarSignResult result) {
            work.SignResult = std::move(result);
            work.HasSignResult = true;
            WipePasswordBuffers(work);
        }

        void PollJarSigner(Context &context) {
            PollRetired();
            auto &work = context.JarSignerWork;
            if (work.Busy.load()) {
                JarVerifyResult verify;
                KeytoolAliasListResult aliases;
                JarSignResult sign;
                JarSignResult signFailed;
                signFailed.Status = JarSignStatus::SignFailed;
                if (TakeFinished(work.VerifyFuture, verify, JarVerifyResult{})) {
                    FinishTask(work);
                    ApplyVerify(work, std::move(verify));
                } else if (TakeFinished(work.AliasFuture, aliases, KeytoolAliasListResult{})) {
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
            const EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.HasVerify = false;
            work.Verify = {};
            ClearSignResult(work);
            work.VerifyFuture = std::async(std::launch::async, [jarsigner, archive, toolEnv] {
                return VerifyJar(jarsigner, archive, toolEnv);
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
            WipeSecret(storePassword);
            const JdkInfo jdk = context.Host.Jdk;
            const std::string keystore = work.KeystorePath;
            const EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.AliasFuture = std::async(
                std::launch::async,
                [secret = std::move(secret), jdk, keystore, toolEnv]() mutable {
                    KeytoolAliasListResult result = ListKeystoreAliases(jdk, keystore, std::move(secret.Value), toolEnv);
                    WipeSecret(secret.Value);
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
            JarSignRequest request;
            request.InputPath = work.InputPath;
            request.OutputPath = work.OutputPath;
            request.KeystorePath = work.KeystorePath;
            request.KeystoreType = work.KeystoreType;
            request.KeyAlias = work.SelectedAlias;
            request.Jdk = context.Host.Jdk;
            request.ToolEnv = context.Host.Sdk.ToolEnv;
            request.StorePassword = std::move(storePassword);
            WipeSecret(storePassword);
            request.KeyPassword = std::move(keyPassword);
            WipeSecret(keyPassword);

            SignLaunch launch{std::move(request)};
            WipeSecret(request.StorePassword);
            WipeSecret(request.KeyPassword);

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
                Tr("File Name"),
                Tr("Full Path"),
                Tr("Signed By"),
                Tr("Digest Algorithm"),
                Tr("Signature Algorithm"),
                Tr("Timestamp"),
            };
        }

        void DrawVerifyDetails(const JarVerifyResult &result) {
            if (!result.Started || result.ToolMissing) {
                return;
            }
            const auto columns = ReportColumns();
            LabeledValue(Tr("Signed By"), result.Report.Signer, columns);
            ImGui::Spacing();
            LabeledValue(Tr("Digest Algorithm"), result.Report.DigestAlgorithm, columns);
            ImGui::Spacing();
            LabeledValue(Tr("Signature Algorithm"), result.Report.SignatureAlgorithm, columns);
            ImGui::Spacing();
            LabeledValue(Tr("Timestamp"), result.Report.Timestamp, columns);
        }

        void DrawSignatureReport(const char *title, const std::string &path, const JarVerifyResult &verify) {
            const bool hasStatus = verify.Started && !verify.ToolMissing;
            ReportHeading(title, hasStatus ? &verify.Report.Verified : nullptr);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            FileRows(path, columns);
            ImGui::Spacing();
            DrawVerifyDetails(verify);
        }

        void AliasCombo(JarSignerWork &work, const float width) {
            const bool reading = work.Busy.load() && work.Task == JarSignerTask::Aliases;
            const char *preview = Tr("Select an Alias");
            if (reading) {
                preview = Tr("Reading keystore aliases...");
            } else if (!work.SelectedAlias.empty()) {
                preview = work.SelectedAlias.c_str();
            }
            std::vector<const char *> labels;
            labels.reserve(work.Aliases.size());
            int selected = -1;
            for (int index = 0; index < static_cast<int>(work.Aliases.size()); ++index) {
                const std::string &alias = work.Aliases.at(static_cast<std::size_t>(index));
                labels.push_back(alias.c_str());
                if (alias == work.SelectedAlias) {
                    selected = index;
                }
            }
            ImGui::BeginDisabled(work.Aliases.empty());
            if (RoundedCombo("##Alias", preview, labels.data(), static_cast<int>(labels.size()), selected, width, Tr("Alias"))) {
                if (selected >= 0 && selected < static_cast<int>(work.Aliases.size())) {
                    const std::string &alias = work.Aliases.at(static_cast<std::size_t>(selected));
                    if (work.SelectedAlias != alias) {
                        ClearSignResult(work);
                    }
                    work.SelectedAlias = alias;
                }
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
                return Tr("keytool was not found in the JDK.");
            }
            if (work.AliasResult.Status != KeytoolAliasStatus::Ok) {
                tone = StatusBarTone::Error;
                return Tr("The keystore aliases could not be read.");
            }
            if (work.Aliases.empty()) {
                tone = StatusBarTone::Warning;
                return Tr("No private-key aliases were found in this keystore.");
            }
            return nullptr;
        }

        void DrawSignerInputs(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const char *idleLabel = Tr("Sign");
            const char *busyLabel = Tr("Signing...");
            const float signW = BusyActionWidth(idleLabel, busyLabel);
            const float pathW = std::max(1.0F, (avail - signW - (spacing * 2.0F)) * 0.5F);

            bool refreshAliases = false;
            SecretEdit storePassword;
            {
                FormDisabled disabled(work.Busy.load());
                if (BrowseRow("##ArchivePathField", work.InputPath, Tr("Choose an archive..."), "ArchivePath", Tr("Browse..."), pathW, Tr("Archive"))) {
                    const std::string start = DirectoryOf(work.InputPath);
                    if (const auto picked = FileDialog::PickFile(Tr("Open Archive"), Tr("Java archives"), JarArchivePatterns(), start)) {
                        work.InputPath = *picked;
                        if (!work.OutputCustomized) {
                            work.OutputPath = SuggestedSignedJarPath(work.InputPath);
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
                        Tr("Choose a keystore file..."),
                        "KeystorePath",
                        Tr("Browse..."),
                        pathW,
                        Tr("Keystore")
                    )) {
                    const std::string start = work.KeystorePath.empty() ? DirectoryOf(work.InputPath) : DirectoryOf(work.KeystorePath);
                    if (const auto picked = FileDialog::PickFile(Tr("Open Keystore"), Tr("Keystores"), KeystoreFilePatterns(), start)) {
                        work.KeystorePath = *picked;
                        work.KeystoreType = DetectKeystoreType(*picked);
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
                FormDisabled disabled(work.Busy.load());
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
                    Tr("Keystore Password"),
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
                        Tr("Key Password"),
                        secretW
                    );
                    ImGui::PopID();
                }

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
                }

                ImGui::Spacing();
                if (BrowseRow("##OutputPathField", work.OutputPath, Tr("No output path"), "OutputPath", Tr("Save As..."), -1.0F, Tr("Signed archive"))) {
                    const std::string suggested = work.OutputPath.empty() ? SuggestedSignedJarPath(work.InputPath) : work.OutputPath;
                    if (const auto picked = FileDialog::SaveFile(
                            Tr("Save signed archive"),
                            suggested,
                            Tr("Java archives"),
                            JarArchivePatterns()
                        )) {
                        work.OutputPath = *picked;
                        work.OutputCustomized = true;
                        ClearSignResult(work);
                    }
                }
            }
            if (refreshAliases) {
                StartAliasList(context);
            }
        }

        const char *SignFailure(const JarSignResult &result) {
            switch (result.Status) {
                case JarSignStatus::Refused:
                    return RefusalText(result.Refusal);
                case JarSignStatus::SignFailed:
                    return Tr("Signing failed.");
                case JarSignStatus::Signed:
                    break;
            }
            return Tr("Signing failed.");
        }

        void DrawSignerStatus(Context &context, const std::string &jarsigner) {
            const auto &work = context.JarSignerWork;
            const bool busy = work.Busy.load();
            if (busy && work.Task == JarSignerTask::Sign) {
                StatusBar(StatusBarTone::Busy, Tr("Signing the archive."));
                return;
            }
            if (busy && work.Task == JarSignerTask::Verify) {
                StatusBar(StatusBarTone::Busy, Tr("Checking the signature."));
                return;
            }
            if (busy && work.Task == JarSignerTask::Aliases) {
                StatusBar(StatusBarTone::Busy, Tr("Reading the keystore aliases."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != JarSignStatus::Signed) {
                StatusBar(StatusBarTone::Error, SignFailure(work.SignResult));
                return;
            }
            if (work.HasSignResult) {
                if (!work.SignResult.Verify.Started || work.SignResult.Verify.ToolMissing) {
                    StatusBar(StatusBarTone::Error, Tr("The signature could not be checked."));
                    return;
                }
                StatusBar(StatusBarTone::Success, Tr("Archive signed."));
                return;
            }
            if (!work.InputPath.empty() && work.HasVerify && (!work.Verify.Started || work.Verify.ToolMissing)) {
                StatusBar(StatusBarTone::Error, Tr("The signature could not be checked."));
                return;
            }
            StatusBarTone aliasTone = StatusBarTone::Error;
            if (const char *aliasMessage = AliasNotice(context, aliasTone)) {
                StatusBar(aliasTone, aliasMessage);
                return;
            }
            const JarSignRefusal refusal = SignRefusal(context, work, jarsigner);
            if (ShowLiveRefusal(refusal, work)) {
                StatusBar(StatusBarTone::Error, RefusalText(refusal));
                return;
            }
            if (work.InputPath.empty()) {
                StatusBar(StatusBarTone::Idle, Tr("Choose an archive to see its signature."));
                return;
            }
            if (work.HasVerify && work.Verify.Started && !work.Verify.ToolMissing && work.Verify.Report.Verified) {
                StatusBar(
                    StatusBarTone::Warning,
                    Tr(
                        "This archive is already signed. The new file carries a different signature, "
                        "and the original archive is left unchanged."
                    )
                );
                return;
            }
            StatusBar(StatusBarTone::Idle, Tr("Ready to sign an archive."));
        }

        float ReportCardHeight() {
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float stack = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - (gap * 2.0F));
            return std::max(1.0F, stack * 0.5F);
        }

        void DrawInputDetail(const JarSignerWork &work) {
            if (work.InputPath.empty()) {
                CenteredMessage(Tr("No Archive Selected"), Tr("Choose an archive to see its signature."));
                return;
            }
            if (work.Busy.load() && work.Task == JarSignerTask::Verify) {
                CenteredMessage(Tr("Checking the signature..."), nullptr);
                return;
            }
            if (!work.HasVerify || !work.Verify.Started || work.Verify.ToolMissing) {
                CenteredMessage(Tr("Something Went Wrong!"), Tr("The signature could not be checked."), Colors::NEGATIVE);
                return;
            }
            DrawSignatureReport(Tr("Archive"), work.InputPath, work.Verify);
        }

        void DrawOutputDetail(const JarSignerWork &work) {
            const bool signing = work.Busy.load() && work.Task == JarSignerTask::Sign;
            if (signing) {
                CenteredMessage(Tr("Signing..."), nullptr);
                return;
            }
            if (!work.HasSignResult && work.OutputPath.empty()) {
                CenteredMessage(Tr("No Signed Archive"), Tr("Sign an archive to see the result here."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != JarSignStatus::Signed) {
                CenteredMessage(Tr("Something Went Wrong!"), SignFailure(work.SignResult), Colors::NEGATIVE);
                return;
            }
            if (!work.HasSignResult) {
                ReportHeading(Tr("Signed Archive"));
                ImGui::Spacing();
                const auto columns = ReportColumns();
                FileRows(work.OutputPath, columns);
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("Sign the archive to see its signature here."));
                return;
            }

            DrawSignatureReport(Tr("Signed Archive"), work.OutputPath, work.SignResult.Verify);
        }

        void DrawSignerForm(Context &context, const std::string &jarsigner) {
            auto &work = context.JarSignerWork;
            DrawSignerInputs(context, jarsigner);
            ImGui::Spacing();
            const float cardH = ReportCardHeight();
            DetailCard("##InputDetail", cardH, [&work] { DrawInputDetail(work); });
            DetailCard("##OutputDetail", cardH, [&work] { DrawOutputDetail(work); });
            DrawSignerStatus(context, jarsigner);
        }

        void DrawMissingJarsigner(Context &context) {
            const bool jdkMissing = context.Host.Jdk.JavaHome.empty();
            if (NoticeCard(
                    NoticeCardTone::Error,
                    jdkMissing ? Tr("A JDK is required.") : Tr("jarsigner was not found in the JDK."),
                    Tr("Java Jar Signer signs and verifies archives with the JDK's jarsigner."),
                    Tr("Choose a JDK...")
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
        SetNativeWindowPage(context, Tr("Java Jar Signer"));

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

        const std::string jarsigner = JarsignerPath(context.Host.Jdk);
        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##JarSignerBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        if (context.Host.Jdk.JavaHome.empty() || JarsignerMissing(jarsigner)) {
            DrawMissingJarsigner(context);
        } else {
            DrawSignerForm(context, jarsigner);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseJarSigner(Context &context) {
        LeaveJarSigner(context);
    }
}

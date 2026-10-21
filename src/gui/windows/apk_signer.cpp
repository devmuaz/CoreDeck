//
// Created by AbdulMuaz Aqeel on 14/10/2026.
//

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "apk_signer.h"
#include "onboarding.h"
#include "../theme.h"
#include "../utilities.h"
#include "../widgets.h"
#include "../../core/apk_signer.h"
#include "../../core/keytool.h"
#include "../../core/file_dialog.h"
#include "../../core/paths.h"
#include "../../core/i18n.h"

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
            ApkSignRequest Request;

            explicit SignLaunch(ApkSignRequest request) : Request(std::move(request)) {
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

            ApkSignResult Run() {
                ApkSignResult result = SignApk(std::move(Request));
                WipeSecret(Request.StorePassword);
                WipeSecret(Request.KeyPassword);
                return result;
            }
        };

        struct RetiredJobs {
            std::vector<std::future<ApkVerifyResult>> Verify;
            std::vector<std::future<KeytoolAliasListResult>> Aliases;
            std::vector<std::future<ApkSignResult>> Sign;
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

        void FinishTask(struct Context::ApkSignerWork &work) {
            work.Busy = false;
            work.Task = ApkSignerTask::None;
        }

        void RetireJobs(struct Context::ApkSignerWork &work) {
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

        std::string DefaultSignedPath(const std::string &apkPath) {
            const std::filesystem::path input(apkPath);
            std::string stem = input.stem().string();
            if (stem.empty()) {
                stem = input.filename().string();
            }
            if (stem.empty()) {
                stem = "signed";
            }
            return (input.parent_path() / (stem + "-signed.apk")).string();
        }

        bool ZipalignUnavailable(const std::string &path) {
            if (path.empty()) {
                return true;
            }
            std::error_code error;
            return !std::filesystem::is_regular_file(path, error);
        }

        void WipePasswordBuffers(struct Context::ApkSignerWork &work) {
            WipeSecret(work.StorePassword, sizeof work.StorePassword);
            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.SecretEpoch += 1;
        }

        void ClearSignResult(struct Context::ApkSignerWork &work) {
            work.HasSignResult = false;
            work.SignResult = {};
        }

        void ResetForm(struct Context::ApkSignerWork &work) {
            WipePasswordBuffers(work);
            work.ApkPath.clear();
            work.KeystorePath.clear();
            work.KeystoreType.clear();
            work.OutputPath.clear();
            work.OutputCustomized = false;
            work.KeyPasswordMatches = true;
            work.ShowStorePassword = false;
            work.ShowKeyPassword = false;
            work.Align = true;
            work.HasVerify = false;
            work.Verify = {};
            work.HasAliasResult = false;
            work.AliasResult = {};
            work.Aliases.clear();
            work.SelectedAlias.clear();
            work.AliasListScheduled = false;
            ClearSignResult(work);
        }

        void LeaveApkSigner(Context &context) {
            auto &work = context.ApkSignerWork;
            RetireJobs(work);
            work.Busy = false;
            work.Task = ApkSignerTask::None;
            work.AliasRefreshPending = false;
            ResetForm(work);
            context.UI.Workspace = WorkspacePage::Home;
        }

        const char *RefusalText(const ApkSignRefusal refusal) {
            switch (refusal) {
                case ApkSignRefusal::EmptyApksigner:
                    return Tr("apksigner is not installed.");
                case ApkSignRefusal::MissingJdk:
                    return Tr("A JDK is required.");
                case ApkSignRefusal::MissingZipalign:
                    return Tr("zipalign was not found under build-tools.");
                case ApkSignRefusal::MissingOutput:
                    return Tr("Choose an output path for the signed APK.");
                case ApkSignRefusal::OutputSameAsInput:
                    return Tr("The output path is the same as the APK. The original APK is left unchanged.");
                case ApkSignRefusal::None:
                    break;
            }
            return "";
        }

        ApkSignRefusal SignRefusal(
            const Context &context,
            const struct Context::ApkSignerWork &work,
            const std::string &apksigner,
            const std::string &zipalign
        ) {
            if (apksigner.empty()) {
                return ApkSignRefusal::EmptyApksigner;
            }
            if (context.Host.Jdk.JavaHome.empty()) {
                return ApkSignRefusal::MissingJdk;
            }
            if (work.OutputPath.empty()) {
                return ApkSignRefusal::MissingOutput;
            }
            if (Paths::SameFilePath(work.ApkPath, work.OutputPath)) {
                return ApkSignRefusal::OutputSameAsInput;
            }
            if (work.Align && ZipalignUnavailable(zipalign)) {
                return ApkSignRefusal::MissingZipalign;
            }
            return ApkSignRefusal::None;
        }

        bool ShowLiveRefusal(const ApkSignRefusal refusal, const struct Context::ApkSignerWork &work) {
            switch (refusal) {
                case ApkSignRefusal::None:
                case ApkSignRefusal::EmptyApksigner:
                    return false;
                case ApkSignRefusal::MissingOutput:
                    return !work.ApkPath.empty();
                case ApkSignRefusal::MissingJdk:
                case ApkSignRefusal::MissingZipalign:
                case ApkSignRefusal::OutputSameAsInput:
                    return true;
            }
            return false;
        }

        bool CanSign(
            const Context &context,
            const struct Context::ApkSignerWork &work,
            const std::string &apksigner,
            const std::string &zipalign
        ) {
            if (work.Busy.load() || work.ApkPath.empty() || work.KeystorePath.empty() || work.SelectedAlias.empty()) {
                return false;
            }
            if (work.StorePassword[0] == '\0') {
                return false;
            }
            if (!work.KeyPasswordMatches && work.KeyPassword[0] == '\0') {
                return false;
            }
            return SignRefusal(context, work, apksigner, zipalign) == ApkSignRefusal::None;
        }

        void StartAliasList(Context &context);

        void ApplyVerify(struct Context::ApkSignerWork &work, ApkVerifyResult result) {
            work.Verify = std::move(result);
            work.HasVerify = true;
        }

        void ApplyAlias(struct Context::ApkSignerWork &work, KeytoolAliasListResult result) {
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

        void ApplySign(struct Context::ApkSignerWork &work, ApkSignResult result) {
            work.SignResult = std::move(result);
            work.HasSignResult = true;
            WipePasswordBuffers(work);
        }

        void PollApkSigner(Context &context) {
            PollRetired();
            auto &work = context.ApkSignerWork;
            if (work.Busy.load()) {
                ApkVerifyResult verify;
                KeytoolAliasListResult aliases;
                ApkSignResult sign;
                ApkSignResult signFailed;
                signFailed.Status = ApkSignStatus::SignFailed;
                if (TakeFinished(work.VerifyFuture, verify, ApkVerifyResult{})) {
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

        void StartVerify(Context &context, const std::string &apksigner) {
            auto &work = context.ApkSignerWork;
            if (work.Busy.load() || work.ApkPath.empty() || apksigner.empty()) {
                return;
            }
            const std::string apk = work.ApkPath;
            const EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.HasVerify = false;
            work.Verify = {};
            ClearSignResult(work);
            work.VerifyFuture = std::async(std::launch::async, [apksigner, apk, toolEnv] {
                return VerifyApk(apksigner, apk, toolEnv);
            });
            work.Task = ApkSignerTask::Verify;
            work.Busy = true;
        }

        void StartAliasList(Context &context) {
            auto &work = context.ApkSignerWork;
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
            work.Task = ApkSignerTask::Aliases;
            work.Busy = true;
        }

        void StartSign(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            if (work.Busy.load() || !CanSign(context, work, apksigner, zipalign)) {
                return;
            }

            std::string storePassword(work.StorePassword);
            std::string keyPassword = work.KeyPasswordMatches ? storePassword : std::string(work.KeyPassword);
            ApkSignRequest request;
            request.ApksignerPath = apksigner;
            request.ZipalignPath = zipalign;
            request.InputPath = work.ApkPath;
            request.OutputPath = work.OutputPath;
            request.KeystorePath = work.KeystorePath;
            request.KeystoreType = work.KeystoreType;
            request.KeyAlias = work.SelectedAlias;
            request.Align = work.Align;
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
            work.Task = ApkSignerTask::Sign;
            work.Busy = true;
        }

        std::array<const char *, 4> ReportColumns() {
            return {
                Tr("File Name"),
                Tr("Full Path"),
                Tr("Schemes"),
                Tr("Certificate DN"),
            };
        }

        std::string FormatCertificateSha256(const std::string &digest) {
            std::string hex;
            hex.reserve(digest.size());
            for (const unsigned char c: digest) {
                if (std::isxdigit(c)) {
                    hex.push_back(static_cast<char>(std::toupper(c)));
                }
            }
            if (hex.size() < 2) {
                return digest;
            }
            std::string formatted;
            formatted.reserve(hex.size() + (hex.size() / 2));
            for (std::size_t i = 0; i < hex.size(); ++i) {
                if (i > 0 && (i % 2) == 0) {
                    formatted.push_back(':');
                }
                formatted.push_back(hex.at(i));
            }
            return formatted;
        }

        void DrawSchemeChips(const ApkVerifyReport &report) {
            const std::pair<const char *, bool> schemes[] = {
                {Tr("v1"), report.SchemeV1},
                {Tr("v2"), report.SchemeV2},
                {Tr("v3"), report.SchemeV3},
                {Tr("v3.1"), report.SchemeV31},
                {Tr("v4"), report.SchemeV4},
            };
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const auto columns = ReportColumns();
            const float labelColumn = DetailColumnWidth(columns, avail);
            const float padX = 10.0F;
            const float padY = 4.0F;
            const float valueX = labelColumn + spacing;
            const float valueWidth = std::max(1.0F, avail - valueX);

            struct Chip {
                const char *Text;
                bool On;
                float Width;
                float Height;
            };
            Chip chips[IM_ARRAYSIZE(schemes)];
            float chipH = 0.0F;
            for (int index = 0; index < IM_ARRAYSIZE(schemes); ++index) {
                const ImVec2 text = ImGui::CalcTextSize(schemes[index].first);
                chips[index].Text = schemes[index].first;
                chips[index].On = schemes[index].second;
                chips[index].Width = text.x + (padX * 2.0F);
                chips[index].Height = text.y + (padY * 2.0F);
                chipH = std::max(chipH, chips[index].Height);
            }

            int lineStart[IM_ARRAYSIZE(schemes) + 1];
            int lineCount = 0;
            lineStart[0] = 0;
            float lineWidth = 0.0F;
            for (int index = 0; index < IM_ARRAYSIZE(schemes); ++index) {
                const float gap = lineWidth > 0.0F ? spacing : 0.0F;
                if (lineWidth > 0.0F && lineWidth + gap + chips[index].Width > valueWidth) {
                    lineCount += 1;
                    lineStart[lineCount] = index;
                    lineWidth = chips[index].Width;
                } else {
                    lineWidth += gap + chips[index].Width;
                }
            }
            lineCount += 1;
            lineStart[lineCount] = IM_ARRAYSIZE(schemes);

            const float blockH = (chipH * static_cast<float>(lineCount)) + (spacing * static_cast<float>(lineCount - 1));
            const float rowH = std::max(ImGui::GetTextLineHeight(), blockH);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(avail, rowH));

            ImDrawList *draw = ImGui::GetWindowDrawList();
            const float labelY = origin.y + ((rowH - ImGui::GetTextLineHeight()) * 0.5F);
            DetailLabel(origin, labelColumn, rowH, Tr("Schemes"), labelY);

            float chipY = origin.y + ((rowH - blockH) * 0.5F);
            for (int line = 0; line < lineCount; ++line) {
                float chipX = origin.x + valueX;
                for (int index = lineStart[line]; index < lineStart[line + 1]; ++index) {
                    const Chip &chip = chips[index];
                    const float top = chipY + ((chipH - chip.Height) * 0.5F);
                    const ImVec2 chipMin(chipX, top);
                    const ImVec2 chipMax(chipX + chip.Width, top + chip.Height);
                    const float rounding = chip.Height * 0.5F;
                    const char *accent = chip.On ? Colors::ACCENT_INFO : Colors::TEXT_HINT;
                    const ImU32 fill = chip.On
                                           ? ImGui::GetColorU32(HexColor(Colors::ACCENT_INFO, 0.16F))
                                           : ImGui::GetColorU32(HexColor(Colors::SURFACE2));
                    const ImU32 border = chip.On
                                             ? ImGui::GetColorU32(HexColor(Colors::ACCENT_INFO, 0.35F))
                                             : ImGui::GetColorU32(HexColor(Colors::SURFACE4));
                    draw->AddRectFilled(chipMin, chipMax, fill, rounding);
                    draw->AddRect(chipMin, chipMax, border, rounding, 0, 1.0F);
                    const ImVec2 text = ImGui::CalcTextSize(chip.Text);
                    draw->AddText(
                        ImVec2(chipX + padX, top + ((chip.Height - text.y) * 0.5F)),
                        ImGui::GetColorU32(HexColor(accent)),
                        chip.Text
                    );
                    chipX += chip.Width + spacing;
                }
                chipY += chipH + spacing;
            }
        }

        void DrawVerifyDetails(const ApkVerifyResult &result) {
            if (!result.Started || result.ToolMissing) {
                return;
            }
            DrawSchemeChips(result.Report);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            LabeledValue(Tr("Certificate DN"), result.Report.SignerDn, columns);
            ImGui::Spacing();
            CopyableField(Tr("Certificate SHA-256"), FormatCertificateSha256(result.Report.CertificateSha256));
        }

        void DrawSignatureReport(const char *title, const std::string &path, const ApkVerifyResult &verify) {
            const bool hasStatus = verify.Started && !verify.ToolMissing;
            ReportHeading(title, hasStatus ? &verify.Report.Verified : nullptr);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            FileRows(path, columns);
            ImGui::Spacing();
            DrawVerifyDetails(verify);
        }

        void AliasCombo(struct Context::ApkSignerWork &work, const float width) {
            const bool reading = work.Busy.load() && work.Task == ApkSignerTask::Aliases;
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
            const auto &work = context.ApkSignerWork;
            const bool reading = work.Busy.load() && work.Task == ApkSignerTask::Aliases;
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

        void DrawSignerInputs(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const ImGuiStyle &style = ImGui::GetStyle();
            const char *idleLabel = Tr("Sign");
            const char *busyLabel = Tr("Signing...");
            const float signW = BusyActionWidth(idleLabel, busyLabel);
            const float pathW = std::max(1.0F, (avail - signW - (spacing * 2.0F)) * 0.5F);

            bool refreshAliases = false;
            SecretEdit storePassword;
            {
                FormDisabled disabled(work.Busy.load());
                if (BrowseRow("##ApkPathField", work.ApkPath, Tr("Choose an APK file..."), "ApkPath", Tr("Browse..."), pathW, Tr("APK"))) {
                    const std::string start = DirectoryOf(work.ApkPath);
                    if (const auto picked = FileDialog::PickFile(Tr("Open APK"), Tr("Android packages"), {"*.apk", "*.APK"}, start)) {
                        work.ApkPath = *picked;
                        if (!work.OutputCustomized) {
                            work.OutputPath = DefaultSignedPath(work.ApkPath);
                        }
                        work.HasVerify = false;
                        work.Verify = {};
                        ClearSignResult(work);
                        StartVerify(context, apksigner);
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
                    const std::string start = work.KeystorePath.empty() ? DirectoryOf(work.ApkPath) : DirectoryOf(work.KeystorePath);
                    if (const auto picked = FileDialog::PickFile(Tr("Open Keystore"), Tr("Keystores"), KeystoreFilePatterns(), start)) {
                        work.KeystorePath = *picked;
                        work.KeystoreType = DetectKeystoreType(*picked);
                        ClearSignResult(work);
                        refreshAliases = true;
                    }
                }
            }
            ImGui::SameLine(0.0F, spacing);
            const bool signing = work.Busy.load() && work.Task == ApkSignerTask::Sign;
            if (BusyButton("##SignApk", idleLabel, busyLabel, signing, CanSign(context, work, apksigner, zipalign), signW)) {
                StartSign(context, apksigner, zipalign);
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
                const char *alignTitle = Tr("Align before signing");
                const char *alignSubtitle = Tr("Zip-align the APK before the signature is written.");
                const char *matchTitle = Tr("Key password matches keystore password");
                const char *matchSubtitle = Tr("Turn this off if the key entry has a different password.");
                const float box = 28.0F;
                const auto blockWidth = [&](const char *title, const char *subtitle) {
                    const float text = std::max(ImGui::CalcTextSize(title).x, ImGui::CalcTextSize(subtitle).x);
                    return box + style.ItemInnerSpacing.x + text;
                };
                const bool checksFit = blockWidth(alignTitle, alignSubtitle) + spacing + blockWidth(matchTitle, matchSubtitle) <= avail;
                if (SubtitledCheckbox("##AlignBeforeSigning", &work.Align, alignTitle, alignSubtitle)) {
                    ClearSignResult(work);
                }
                if (checksFit) {
                    ImGui::SameLine(0.0F, spacing);
                }
                if (SubtitledCheckbox("##KeyPasswordMatches", &work.KeyPasswordMatches, matchTitle, matchSubtitle)) {
                    if (work.KeyPasswordMatches) {
                        WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
                        work.ShowKeyPassword = false;
                        work.SecretEpoch += 1;
                    }
                }

                ImGui::Spacing();
                if (BrowseRow("##OutputPathField", work.OutputPath, Tr("No output path"), "OutputPath", Tr("Save As..."), -1.0F, Tr("Signed APK"))) {
                    const std::string suggested = work.OutputPath.empty() ? DefaultSignedPath(work.ApkPath) : work.OutputPath;
                    if (const auto picked = FileDialog::SaveFile(
                            Tr("Save signed APK"),
                            suggested,
                            Tr("Android packages"),
                            {"*.apk", "*.APK"}
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

        const char *SignFailure(const ApkSignResult &result) {
            switch (result.Status) {
                case ApkSignStatus::Refused:
                    return RefusalText(result.Refusal);
                case ApkSignStatus::AlignFailed:
                    return Tr("Alignment failed.");
                case ApkSignStatus::SignFailed:
                    return Tr("Signing failed.");
                case ApkSignStatus::Signed:
                    break;
            }
            return Tr("Signing failed.");
        }

        void DrawSignerStatus(Context &context, const std::string &apksigner, const std::string &zipalign) {
            const auto &work = context.ApkSignerWork;
            const bool busy = work.Busy.load();
            if (busy && work.Task == ApkSignerTask::Sign) {
                StatusBar(StatusBarTone::Busy, Tr("Signing the APK."));
                return;
            }
            if (busy && work.Task == ApkSignerTask::Verify) {
                StatusBar(StatusBarTone::Busy, Tr("Checking the signature."));
                return;
            }
            if (busy && work.Task == ApkSignerTask::Aliases) {
                StatusBar(StatusBarTone::Busy, Tr("Reading the keystore aliases."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != ApkSignStatus::Signed) {
                StatusBar(StatusBarTone::Error, SignFailure(work.SignResult));
                return;
            }
            if (work.HasSignResult) {
                if (!work.SignResult.Verify.Started || work.SignResult.Verify.ToolMissing) {
                    StatusBar(StatusBarTone::Error, Tr("The signature could not be checked."));
                    return;
                }
                StatusBar(StatusBarTone::Success, Tr("APK signed."));
                return;
            }
            if (!work.ApkPath.empty() && work.HasVerify && (!work.Verify.Started || work.Verify.ToolMissing)) {
                StatusBar(StatusBarTone::Error, Tr("The signature could not be checked."));
                return;
            }
            StatusBarTone aliasTone = StatusBarTone::Error;
            if (const char *aliasMessage = AliasNotice(context, aliasTone)) {
                StatusBar(aliasTone, aliasMessage);
                return;
            }
            const ApkSignRefusal refusal = SignRefusal(context, work, apksigner, zipalign);
            if (ShowLiveRefusal(refusal, work)) {
                StatusBar(StatusBarTone::Error, RefusalText(refusal));
                return;
            }
            if (work.ApkPath.empty()) {
                StatusBar(StatusBarTone::Idle, Tr("Choose an APK to see its signature."));
                return;
            }
            if (work.HasVerify && work.Verify.Started && !work.Verify.ToolMissing && work.Verify.Report.Verified) {
                StatusBar(
                    StatusBarTone::Warning,
                    Tr(
                        "This APK is already signed. The new file carries a different signature, "
                        "and the original APK is left unchanged."
                    )
                );
                return;
            }
            StatusBar(StatusBarTone::Idle, Tr("Ready to sign an APK."));
        }

        float ReportCardHeight() {
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float stack = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - (gap * 2.0F));
            return std::max(1.0F, stack * 0.5F);
        }

        void DrawInputDetail(const struct Context::ApkSignerWork &work) {
            if (work.ApkPath.empty()) {
                CenteredMessage(Tr("No APK Selected"), Tr("Choose an APK to see its signature."));
                return;
            }
            if (work.Busy.load() && work.Task == ApkSignerTask::Verify) {
                CenteredMessage(Tr("Checking the signature..."), nullptr);
                return;
            }
            if (!work.HasVerify || !work.Verify.Started || work.Verify.ToolMissing) {
                CenteredMessage(Tr("Something Went Wrong!"), Tr("The signature could not be checked."), Colors::NEGATIVE);
                return;
            }
            DrawSignatureReport(Tr("APK"), work.ApkPath, work.Verify);
        }

        void DrawOutputDetail(const struct Context::ApkSignerWork &work) {
            const bool signing = work.Busy.load() && work.Task == ApkSignerTask::Sign;
            if (signing) {
                CenteredMessage(Tr("Signing..."), nullptr);
                return;
            }
            if (!work.HasSignResult && work.OutputPath.empty()) {
                CenteredMessage(Tr("No Signed APK"), Tr("Sign an APK to see the result here."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != ApkSignStatus::Signed) {
                CenteredMessage(Tr("Something Went Wrong!"), SignFailure(work.SignResult), Colors::NEGATIVE);
                return;
            }
            if (!work.HasSignResult) {
                ReportHeading(Tr("Signed APK"));
                ImGui::Spacing();
                const auto columns = ReportColumns();
                FileRows(work.OutputPath, columns);
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("Sign the APK to see its signature here."));
                return;
            }

            DrawSignatureReport(Tr("Signed APK"), work.OutputPath, work.SignResult.Verify);
        }

        void DrawSignerForm(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            DrawSignerInputs(context, apksigner, zipalign);
            ImGui::Spacing();
            const float cardH = ReportCardHeight();
            DetailCard("##InputDetail", cardH, [&work] { DrawInputDetail(work); });
            DetailCard("##OutputDetail", cardH, [&work] { DrawOutputDetail(work); });
            DrawSignerStatus(context, apksigner, zipalign);
        }

        void DrawMissingSigner(Context &context) {
            if (NoticeCard(
                    NoticeCardTone::Error,
                    Tr("Missing Build Tools (apksigner & zipalign)"),
                    Tr(
                        "APK Signer uses apksigner from the Android build-tools. Install those tools to add it. "
                        "AVDs already in this SDK stay where they are."
                    ),
                    Tr("Install build-tools...")
                )) {
                LeaveApkSigner(context);
                OpenBuildToolsInstall(context);
            }
        }

    }

    void BuildApkSignerWindow(Context &context) {
        PollApkSigner(context);
        if (context.UI.Workspace != WorkspacePage::ApkSigner) {
            return;
        }
        SetNativeWindowPage(context, Tr("APK Signer"));

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            StyleVar sv;
            sv.Push(ImGuiStyleVar_WindowRounding, 0.0F);
            sv.Push(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::Begin("##ApkSigner", nullptr, WORKSPACE_WINDOW_FLAGS);
        }

        const std::string &apksigner = context.Host.Sdk.ApksignerPath;
        const std::string &zipalign = context.Host.Sdk.ZipalignPath;
        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##ApkSignerBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        if (apksigner.empty()) {
            DrawMissingSigner(context);
        } else {
            DrawSignerForm(context, apksigner, zipalign);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseApkSigner(Context &context) {
        LeaveApkSigner(context);
    }
}

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
#include "app/file_dialog.h"
#include "platform/paths.h"
#include "app/paths.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        struct StorePassword {
            std::string Value;

            explicit StorePassword(std::string value) : Value(std::move(value)) {
                Platform::WipeSecret(value);
            }

            StorePassword(const StorePassword &) = delete;
            StorePassword &operator=(const StorePassword &) = delete;
            StorePassword &operator=(StorePassword &&) = delete;

            StorePassword(StorePassword &&other) noexcept : Value(std::move(other.Value)) {
                Platform::WipeSecret(other.Value);
            }

            ~StorePassword() {
                Platform::WipeSecret(Value);
            }
        };

        struct SignLaunch {
            Core::ApkSignRequest Request;

            explicit SignLaunch(Core::ApkSignRequest request) : Request(std::move(request)) {
                // Moved-from small strings can retain the secret in their inline buffer.
                Platform::WipeSecret(request.StorePassword); // NOLINT(bugprone-use-after-move)
                Platform::WipeSecret(request.KeyPassword); // NOLINT(bugprone-use-after-move)
            }

            SignLaunch(const SignLaunch &) = delete;
            SignLaunch &operator=(const SignLaunch &) = delete;
            SignLaunch &operator=(SignLaunch &&) = delete;

            SignLaunch(SignLaunch &&other) noexcept : Request(std::move(other.Request)) {
                Platform::WipeSecret(other.Request.StorePassword);
                Platform::WipeSecret(other.Request.KeyPassword);
            }

            ~SignLaunch() {
                Platform::WipeSecret(Request.StorePassword);
                Platform::WipeSecret(Request.KeyPassword);
            }

            Core::ApkSignResult Run() {
                Core::ApkSignResult result = Core::SignApk(std::move(Request));
                Platform::WipeSecret(Request.StorePassword);
                Platform::WipeSecret(Request.KeyPassword);
                return result;
            }
        };

        struct RetiredJobs {
            std::vector<std::future<Core::ApkVerifyResult>> Verify;
            std::vector<std::future<Core::KeytoolAliasListResult>> Aliases;
            std::vector<std::future<Core::ApkSignResult>> Sign;
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

        void FinishTask(ApkSignerWork &work) {
            work.Busy = false;
            work.Task = ApkSignerTask::None;
        }

        void RetireJobs(ApkSignerWork &work) {
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

        void WipePasswordBuffers(ApkSignerWork &work) {
            Platform::WipeSecret(work.StorePassword, sizeof work.StorePassword);
            Platform::WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.SecretEpoch += 1;
        }

        void ClearSignResult(ApkSignerWork &work) {
            work.HasSignResult = false;
            work.SignResult = {};
        }

        void ResetForm(ApkSignerWork &work) {
            WipePasswordBuffers(work);
            work.InputPath.clear();
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

        const char *RefusalText(const Core::ApkSignRefusal refusal) {
            switch (refusal) {
                case Core::ApkSignRefusal::EmptyApksigner:
                    return Platform::Tr("apksigner is not installed.");
                case Core::ApkSignRefusal::MissingJdk:
                    return Platform::Tr("A JDK is required.");
                case Core::ApkSignRefusal::MissingZipalign:
                    return Platform::Tr("zipalign was not found under build-tools.");
                case Core::ApkSignRefusal::MissingOutput:
                    return Platform::Tr("Choose an output path for the signed APK.");
                case Core::ApkSignRefusal::OutputSameAsInput:
                    return Platform::Tr("The output path is the same as the APK. The original APK is left unchanged.");
                case Core::ApkSignRefusal::None:
                    break;
            }
            return "";
        }

        Core::ApkSignRefusal SignRefusal(
            const Context &context,
            const ApkSignerWork &work,
            const std::string &apksigner,
            const std::string &zipalign
        ) {
            if (apksigner.empty()) {
                return Core::ApkSignRefusal::EmptyApksigner;
            }
            if (context.Host.Jdk.JavaHome.empty()) {
                return Core::ApkSignRefusal::MissingJdk;
            }
            if (work.OutputPath.empty()) {
                return Core::ApkSignRefusal::MissingOutput;
            }
            if (Platform::SameFilePath(work.InputPath, work.OutputPath)) {
                return Core::ApkSignRefusal::OutputSameAsInput;
            }
            if (work.Align && ZipalignUnavailable(zipalign)) {
                return Core::ApkSignRefusal::MissingZipalign;
            }
            return Core::ApkSignRefusal::None;
        }

        bool ShowLiveRefusal(const Core::ApkSignRefusal refusal, const ApkSignerWork &work) {
            switch (refusal) {
                case Core::ApkSignRefusal::None:
                case Core::ApkSignRefusal::EmptyApksigner:
                    return false;
                case Core::ApkSignRefusal::MissingOutput:
                    return !work.InputPath.empty();
                case Core::ApkSignRefusal::MissingJdk:
                case Core::ApkSignRefusal::MissingZipalign:
                case Core::ApkSignRefusal::OutputSameAsInput:
                    return true;
            }
            return false;
        }

        bool CanSign(
            const Context &context,
            const ApkSignerWork &work,
            const std::string &apksigner,
            const std::string &zipalign
        ) {
            if (work.Busy.load() || work.InputPath.empty() || work.KeystorePath.empty() || work.SelectedAlias.empty()) {
                return false;
            }
            if (work.StorePassword[0] == '\0') {
                return false;
            }
            if (!work.KeyPasswordMatches && work.KeyPassword[0] == '\0') {
                return false;
            }
            return SignRefusal(context, work, apksigner, zipalign) == Core::ApkSignRefusal::None;
        }

        void StartAliasList(Context &context);

        void ApplyVerify(ApkSignerWork &work, Core::ApkVerifyResult result) {
            work.Verify = std::move(result);
            work.HasVerify = true;
        }

        void ApplyAlias(ApkSignerWork &work, Core::KeytoolAliasListResult result) {
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

        void ApplySign(ApkSignerWork &work, Core::ApkSignResult result) {
            work.SignResult = std::move(result);
            work.HasSignResult = true;
            WipePasswordBuffers(work);
        }

        void PollApkSigner(Context &context) {
            PollRetired();
            auto &work = context.ApkSignerWork;
            if (work.Busy.load()) {
                Core::ApkVerifyResult verify;
                Core::KeytoolAliasListResult aliases;
                Core::ApkSignResult sign;
                Core::ApkSignResult signFailed;
                signFailed.Status = Core::ApkSignStatus::SignFailed;
                if (TakeFinished(work.VerifyFuture, verify, Core::ApkVerifyResult{})) {
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

        void StartVerify(Context &context, const std::string &apksigner) {
            auto &work = context.ApkSignerWork;
            if (work.Busy.load() || work.InputPath.empty() || apksigner.empty()) {
                return;
            }
            const std::string apk = work.InputPath;
            const Platform::EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.HasVerify = false;
            work.Verify = {};
            ClearSignResult(work);
            work.VerifyFuture = std::async(std::launch::async, [apksigner, apk, toolEnv] {
                return Core::VerifyApk(apksigner, apk, toolEnv);
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
            Platform::WipeSecret(storePassword);
            const Core::JdkInfo jdk = context.Host.Jdk;
            const std::string keystore = work.KeystorePath;
            const Platform::EnvVars toolEnv = context.Host.Sdk.ToolEnv;
            RetireJobs(work);
            work.AliasFuture = std::async(
                std::launch::async,
                [secret = std::move(secret), jdk, keystore, toolEnv]() mutable {
                    Core::KeytoolAliasListResult result = Core::ListKeystoreAliases(jdk, keystore, std::move(secret.Value), toolEnv);
                    Platform::WipeSecret(secret.Value);
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
            Core::ApkSignRequest request;
            request.ApksignerPath = apksigner;
            request.ZipalignPath = zipalign;
            request.InputPath = work.InputPath;
            request.OutputPath = work.OutputPath;
            request.KeystorePath = work.KeystorePath;
            request.KeystoreType = work.KeystoreType;
            request.KeyAlias = work.SelectedAlias;
            request.Align = work.Align;
            request.Jdk = context.Host.Jdk;
            request.ToolEnv = context.Host.Sdk.ToolEnv;
            request.StorePassword = std::move(storePassword);
            Platform::WipeSecret(storePassword);
            request.KeyPassword = std::move(keyPassword);
            Platform::WipeSecret(keyPassword);

            SignLaunch launch{std::move(request)};
            Platform::WipeSecret(request.StorePassword); // NOLINT(bugprone-use-after-move)
            Platform::WipeSecret(request.KeyPassword); // NOLINT(bugprone-use-after-move)

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
                Platform::Tr("File Name"),
                Platform::Tr("Full Path"),
                Platform::Tr("Schemes"),
                Platform::Tr("Certificate DN"),
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

        void SchemeChips(const Core::ApkVerifyReport &report) {
            const std::pair<const char *, bool> schemes[] = {
                {Platform::Tr("v1"), report.SchemeV1},
                {Platform::Tr("v2"), report.SchemeV2},
                {Platform::Tr("v3"), report.SchemeV3},
                {Platform::Tr("v3.1"), report.SchemeV31},
                {Platform::Tr("v4"), report.SchemeV4},
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
            DetailLabel(origin, labelColumn, rowH, Platform::Tr("Schemes"), labelY);

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

        void VerifyDetails(const Core::ApkVerifyResult &result) {
            if (!result.Started || result.ToolMissing) {
                return;
            }
            SchemeChips(result.Report);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            LabeledValue(Platform::Tr("Certificate DN"), result.Report.SignerDn, columns);
            ImGui::Spacing();
            CopyableField(Platform::Tr("Certificate SHA-256"), FormatCertificateSha256(result.Report.CertificateSha256));
        }

        void SignatureReport(const char *title, const std::string &path, const Core::ApkVerifyResult &verify) {
            const bool hasStatus = verify.Started && !verify.ToolMissing;
            ReportHeading(title, hasStatus ? &verify.Report.Verified : nullptr);
            ImGui::Spacing();
            const auto columns = ReportColumns();
            FileRows(path, columns);
            ImGui::Spacing();
            VerifyDetails(verify);
        }

        void AliasCombo(ApkSignerWork &work, const float width) {
            const bool reading = work.Busy.load() && work.Task == ApkSignerTask::Aliases;
            const char *preview = Platform::Tr("Select an Alias");
            if (reading) {
                preview = Platform::Tr("Reading keystore aliases...");
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
            if (RoundedCombo("##Alias", preview, labels.data(), static_cast<int>(labels.size()), selected, width, Platform::Tr("Alias")) && selected >= 0 && std::cmp_less(selected, work.Aliases.size())) {
                const std::string &alias = work.Aliases.at(static_cast<std::size_t>(selected));
                if (work.SelectedAlias != alias) {
                    ClearSignResult(work);
                }
                work.SelectedAlias = alias;
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
                return Platform::Tr("keytool was not found in the JDK.");
            }
            if (work.AliasResult.Status != Core::KeytoolAliasStatus::Ok) {
                tone = StatusBarTone::Error;
                return Platform::Tr("The keystore aliases could not be read.");
            }
            if (work.Aliases.empty()) {
                tone = StatusBarTone::Warning;
                return Platform::Tr("No private-key aliases were found in this keystore.");
            }
            return nullptr;
        }

        void ApkSignerOptions(ApkSignerWork &work, const float spacing, const float avail) {
            const ImGuiStyle &style = ImGui::GetStyle();
            const char *alignTitle = Platform::Tr("Align before signing");
            const char *alignSubtitle = Platform::Tr("Zip-align the APK before the signature is written.");
            const char *matchTitle = Platform::Tr("Key password matches keystore password");
            const char *matchSubtitle = Platform::Tr("Turn this off if the key entry has a different password.");
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
            if (SubtitledCheckbox("##KeyPasswordMatches", &work.KeyPasswordMatches, matchTitle, matchSubtitle) && work.KeyPasswordMatches) {
                Platform::WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
                work.ShowKeyPassword = false;
                work.SecretEpoch += 1;
            }

            ImGui::Spacing();
            if (BrowseRow("##OutputPathField", work.OutputPath, Platform::Tr("No output path"), "OutputPath", Platform::Tr("Save As..."), -1.0F, Platform::Tr("Signed APK"))) {
                const std::string suggested = work.OutputPath.empty() ? DefaultSignedPath(work.InputPath) : work.OutputPath;
                if (const auto picked = App::FileDialog::SaveFile(
                        Platform::Tr("Save signed APK"),
                        suggested,
                        Platform::Tr("Android packages"),
                        {"*.apk", "*.APK"}
                    )) {
                    work.OutputPath = *picked;
                    work.OutputCustomized = true;
                    ClearSignResult(work);
                }
            }
        }

        void SignerInputs(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const char *idleLabel = Platform::Tr("Sign");
            const char *busyLabel = Platform::Tr("Signing...");
            const float signW = BusyActionWidth(idleLabel, busyLabel);
            const float pathW = std::max(1.0F, (avail - signW - (spacing * 2.0F)) * 0.5F);

            bool refreshAliases = false;
            SecretEdit storePassword;
            {
                const FormDisabled disabled(work.Busy.load());
                if (BrowseRow("##ApkPathField", work.InputPath, Platform::Tr("Choose an APK file..."), "ApkPath", Platform::Tr("Browse..."), pathW, Platform::Tr("APK"))) {
                    const std::string start = DirectoryOf(work.InputPath);
                    if (const auto picked = App::FileDialog::PickFile(Platform::Tr("Open APK"), Platform::Tr("Android packages"), {"*.apk", "*.APK"}, start)) {
                        work.InputPath = *picked;
                        if (!work.OutputCustomized) {
                            work.OutputPath = DefaultSignedPath(work.InputPath);
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
                        Platform::Tr("Choose a keystore file..."),
                        "KeystorePath",
                        Platform::Tr("Browse..."),
                        pathW,
                        Platform::Tr("Keystore")
                    )) {
                    const std::string start = work.KeystorePath.empty() ? DirectoryOf(work.InputPath) : DirectoryOf(work.KeystorePath);
                    if (const auto picked = App::FileDialog::PickFile(Platform::Tr("Open Keystore"), Platform::Tr("Keystores"), Core::KeystoreFilePatterns(), start)) {
                        work.KeystorePath = *picked;
                        work.KeystoreType = Core::DetectKeystoreType(*picked);
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
                    Platform::Tr("Keystore Password"),
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
                        Platform::Tr("Key Password"),
                        secretW
                    );
                    ImGui::PopID();
                }

                ImGui::Spacing();
                ApkSignerOptions(work, spacing, avail);
            }
            if (refreshAliases) {
                StartAliasList(context);
            }
        }

        const char *SignFailure(const Core::ApkSignResult &result) {
            switch (result.Status) {
                case Core::ApkSignStatus::Refused:
                    return RefusalText(result.Refusal);
                case Core::ApkSignStatus::AlignFailed:
                    return Platform::Tr("Alignment failed.");
                case Core::ApkSignStatus::SignFailed:
                    return Platform::Tr("Signing failed.");
                case Core::ApkSignStatus::Signed:
                    break;
            }
            return Platform::Tr("Signing failed.");
        }

        void SignerStatus(const Context &context, const std::string &apksigner, const std::string &zipalign) {
            const auto &work = context.ApkSignerWork;
            const bool busy = work.Busy.load();
            if (busy && work.Task == ApkSignerTask::Sign) {
                StatusBar(StatusBarTone::Busy, Platform::Tr("Signing the APK."));
                return;
            }
            if (busy && work.Task == ApkSignerTask::Verify) {
                StatusBar(StatusBarTone::Busy, Platform::Tr("Checking the signature."));
                return;
            }
            if (busy && work.Task == ApkSignerTask::Aliases) {
                StatusBar(StatusBarTone::Busy, Platform::Tr("Reading the keystore aliases."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != Core::ApkSignStatus::Signed) {
                StatusBar(StatusBarTone::Error, SignFailure(work.SignResult));
                return;
            }
            if (work.HasSignResult) {
                if (!work.SignResult.Verify.Started || work.SignResult.Verify.ToolMissing) {
                    StatusBar(StatusBarTone::Error, Platform::Tr("The signature could not be checked."));
                    return;
                }
                StatusBar(StatusBarTone::Success, Platform::Tr("APK signed."));
                return;
            }
            if (!work.InputPath.empty() && work.HasVerify && (!work.Verify.Started || work.Verify.ToolMissing)) {
                StatusBar(StatusBarTone::Error, Platform::Tr("The signature could not be checked."));
                return;
            }
            StatusBarTone aliasTone = StatusBarTone::Error;
            if (const char *aliasMessage = AliasNotice(context, aliasTone)) {
                StatusBar(aliasTone, aliasMessage);
                return;
            }
            const Core::ApkSignRefusal refusal = SignRefusal(context, work, apksigner, zipalign);
            if (ShowLiveRefusal(refusal, work)) {
                StatusBar(StatusBarTone::Error, RefusalText(refusal));
                return;
            }
            if (work.InputPath.empty()) {
                StatusBar(StatusBarTone::Idle, Platform::Tr("Choose an APK to see its signature."));
                return;
            }
            if (work.HasVerify && work.Verify.Started && !work.Verify.ToolMissing && work.Verify.Report.Verified) {
                StatusBar(
                    StatusBarTone::Warning,
                    Platform::Tr(
                        "This APK is already signed. The new file carries a different signature, "
                        "and the original APK is left unchanged."
                    )
                );
                return;
            }
            StatusBar(StatusBarTone::Idle, Platform::Tr("Ready to sign an APK."));
        }

        float ReportCardHeight() {
            const float gap = ImGui::GetStyle().ItemSpacing.y;
            const float stack = std::max(1.0F, ImGui::GetContentRegionAvail().y - StatusBarHeight() - (gap * 2.0F));
            return std::max(1.0F, stack * 0.5F);
        }

        void InputDetail(const ApkSignerWork &work) {
            if (work.InputPath.empty()) {
                CenteredMessage(Platform::Tr("No APK Selected"), Platform::Tr("Choose an APK to see its signature."));
                return;
            }
            if (work.Busy.load() && work.Task == ApkSignerTask::Verify) {
                CenteredMessage(Platform::Tr("Checking the signature..."), nullptr);
                return;
            }
            if (!work.HasVerify || !work.Verify.Started || work.Verify.ToolMissing) {
                CenteredMessage(Platform::Tr("Something Went Wrong!"), Platform::Tr("The signature could not be checked."), Colors::NEGATIVE);
                return;
            }
            SignatureReport(Platform::Tr("APK"), work.InputPath, work.Verify);
        }

        void OutputDetail(const ApkSignerWork &work) {
            const bool signing = work.Busy.load() && work.Task == ApkSignerTask::Sign;
            if (signing) {
                CenteredMessage(Platform::Tr("Signing..."), nullptr);
                return;
            }
            if (!work.HasSignResult && work.OutputPath.empty()) {
                CenteredMessage(Platform::Tr("No Signed APK"), Platform::Tr("Sign an APK to see the result here."));
                return;
            }
            if (work.HasSignResult && work.SignResult.Status != Core::ApkSignStatus::Signed) {
                CenteredMessage(Platform::Tr("Something Went Wrong!"), SignFailure(work.SignResult), Colors::NEGATIVE);
                return;
            }
            if (!work.HasSignResult) {
                ReportHeading(Platform::Tr("Signed APK"));
                ImGui::Spacing();
                const auto columns = ReportColumns();
                FileRows(work.OutputPath, columns);
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Platform::Tr("Sign the APK to see its signature here."));
                return;
            }

            SignatureReport(Platform::Tr("Signed APK"), work.OutputPath, work.SignResult.Verify);
        }

        void SignerForm(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            SignerInputs(context, apksigner, zipalign);
            ImGui::Spacing();
            const float cardH = ReportCardHeight();
            DetailCard("##InputDetail", cardH, [&work] { InputDetail(work); });
            DetailCard("##OutputDetail", cardH, [&work] { OutputDetail(work); });
            SignerStatus(context, apksigner, zipalign);
        }

        void MissingApkSigner(Context &context) {
            if (NoticeCard(
                    NoticeCardTone::Error,
                    Platform::Tr("Missing Build Tools (apksigner & zipalign)"),
                    Platform::Tr(
                        "APK Signer uses apksigner from the Android build-tools. Install those tools to add it. "
                        "AVDs already in this SDK stay where they are."
                    ),
                    Platform::Tr("Install build-tools...")
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
        SetNativeWindowPage(context, Platform::Tr("APK Signer"));

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
            MissingApkSigner(context);
        } else {
            SignerForm(context, apksigner, zipalign);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void CloseApkSigner(Context &context) {
        LeaveApkSigner(context);
    }
}

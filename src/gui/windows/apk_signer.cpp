//
// Created by AbdulMuaz Aqeel on 14/10/2026.
//

#include <algorithm>
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
#include "../../core/file_dialog.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        constexpr const char *UNAVAILABLE = "\u2014";

        constexpr ImGuiWindowFlags APK_SIGNER_WINDOW_FLAGS =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoNavFocus;

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
            std::vector<std::future<ApkAliasListResult>> Aliases;
            std::vector<std::future<ApkSignResult>> Sign;
        };

        RetiredJobs &Graveyard() {
            static RetiredJobs jobs;
            return jobs;
        }

        template<typename T>
        void Retire(std::future<T> &job, std::vector<std::future<T>> &retired) {
            if (job.valid()) {
                retired.push_back(std::move(job));
            }
        }

        template<typename T>
        void DropFinished(std::vector<std::future<T>> &retired) {
            std::erase_if(retired, [](std::future<T> &job) {
                return job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            });
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

        struct FormDisabled {
            bool Active = false;

            explicit FormDisabled(const bool active) : Active(active) {
                if (Active) {
                    ImGui::BeginDisabled();
                }
            }

            FormDisabled(const FormDisabled &) = delete;
            FormDisabled(FormDisabled &&) = delete;
            FormDisabled &operator=(const FormDisabled &) = delete;
            FormDisabled &operator=(FormDisabled &&) = delete;

            ~FormDisabled() {
                if (Active) {
                    ImGui::EndDisabled();
                }
            }
        };

        std::string DirectoryOf(const std::string &path) {
            if (path.empty()) {
                return {};
            }
            return std::filesystem::path(path).parent_path().string();
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
            WipeSecret(work.StorePassword, sizeof work.StorePassword);
            WipeSecret(work.KeyPassword, sizeof work.KeyPassword);
            work.SecretEpoch += 1;
        }

        void ClearSignResult(ApkSignerWork &work) {
            work.HasSignResult = false;
            work.SignResult = {};
        }

        void ResetForm(ApkSignerWork &work) {
            WipePasswordBuffers(work);
            work.ApkPath.clear();
            work.KeystorePath.clear();
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
            const ApkSignerWork &work,
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
            if (ApkOutputPathEqualsInput(work.ApkPath, work.OutputPath)) {
                return ApkSignRefusal::OutputSameAsInput;
            }
            if (work.Align && ZipalignUnavailable(zipalign)) {
                return ApkSignRefusal::MissingZipalign;
            }
            return ApkSignRefusal::None;
        }

        bool ShowLiveRefusal(const ApkSignRefusal refusal, const ApkSignerWork &work) {
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
            const ApkSignerWork &work,
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

        void ApplyVerify(ApkSignerWork &work, ApkVerifyResult result) {
            work.Verify = std::move(result);
            work.HasVerify = true;
        }

        void ApplyAlias(ApkSignerWork &work, ApkAliasListResult result) {
            work.AliasResult = std::move(result);
            work.HasAliasResult = true;
            work.Aliases.clear();
            if (work.AliasResult.Status != ApkAliasStatus::Ok) {
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

        void ApplySign(ApkSignerWork &work, ApkSignResult result) {
            work.SignResult = std::move(result);
            work.HasSignResult = true;
            WipePasswordBuffers(work);
        }

        void PollApkSigner(Context &context) {
            PollRetired();
            auto &work = context.ApkSignerWork;
            if (work.Busy.load()) {
                if (work.VerifyFuture.valid() &&
                    work.VerifyFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try {
                        ApkVerifyResult result = work.VerifyFuture.get();
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApplyVerify(work, std::move(result));
                    } catch (...) {
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApplyVerify(work, {});
                    }
                } else if (work.AliasFuture.valid() && work.AliasFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try {
                        ApkAliasListResult result = work.AliasFuture.get();
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApplyAlias(work, std::move(result));
                    } catch (...) {
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApkAliasListResult failed;
                        failed.Status = ApkAliasStatus::Failed;
                        ApplyAlias(work, std::move(failed));
                    }
                } else if (work.SignFuture.valid() && work.SignFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try {
                        ApkSignResult result = work.SignFuture.get();
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApplySign(work, std::move(result));
                    } catch (...) {
                        work.Busy = false;
                        work.Task = ApkSignerTask::None;
                        ApkSignResult failed;
                        failed.Status = ApkSignStatus::SignFailed;
                        ApplySign(work, std::move(failed));
                    }
                } else if (!work.VerifyFuture.valid() && !work.AliasFuture.valid() && !work.SignFuture.valid()) {
                    work.Busy = false;
                    work.Task = ApkSignerTask::None;
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
                    ApkAliasListResult result = ListKeystoreAliases(jdk, keystore, std::move(secret.Value), toolEnv);
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

        struct SecretEdit {
            bool Edited = false;
            bool Submitted = false;
        };

        SecretEdit DrawSecretField(const char *id, char *buffer, const std::size_t size, bool &revealed) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonSide = ImGui::GetFrameHeight();
            const float fieldWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x - buttonSide - spacing);
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_NoUndoRedo;
            if (!revealed) {
                flags |= ImGuiInputTextFlags_Password;
            }
            ImGui::SetNextItemWidth(fieldWidth);
            SecretEdit edit;
            edit.Edited = ImGui::InputText(id, buffer, size, flags);
            edit.Submitted = ImGui::IsItemDeactivated() && ImGui::IsKeyDown(ImGuiKey_Enter);
            ImGui::SameLine(0.0F, spacing);
            ImGui::PushID(id);
            if (PrimaryButton(revealed ? Icons::EYE_SLASH : Icons::EYE, true, ImVec2(buttonSide, 0.0F))) {
                revealed = !revealed;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", revealed ? Tr("Hide password") : Tr("Show password"));
            }
            ImGui::PopID();
            return edit;
        }

        void FieldLabel(const char *text) {
            StyleColor sc;
            sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
            ImGui::TextUnformatted(text);
        }

        std::string FileNameOf(const std::string &path) {
            if (path.empty()) {
                return {};
            }
            const std::string name = std::filesystem::path(path).filename().string();
            return name.empty() ? path : name;
        }

        bool BrowseButton(const char *id, const char *label) {
            const float width = std::max(
                Em(12.0F),
                ImGui::CalcTextSize(label).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
            );
            const std::string button = StrConcat(label, "##", id);
            return PrimaryButton(button.c_str(), true, ImVec2(width, 0.0F));
        }

        bool DrawBrowseRow(
            const char *fieldId,
            const std::string &path,
            const char *placeholder,
            const char *browseId,
            const char *browseLabel
        ) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonWidth = std::max(
                Em(12.0F),
                ImGui::CalcTextSize(browseLabel).x + (ImGui::GetStyle().FramePadding.x * 2.0F)
            );
            const float fieldWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x - buttonWidth - spacing);
            std::vector<char> buffer(std::max<std::size_t>(path.size() + 1, 1));
            std::ranges::copy(path, buffer.begin());
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::InputTextWithHint(fieldId, placeholder, buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            return BrowseButton(browseId, browseLabel);
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
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            for (int index = 0; index < static_cast<int>(sizeof(schemes) / sizeof(schemes[0])); ++index) {
                if (index > 0) {
                    ImGui::SameLine(0.0F, gap);
                }
                ImGui::PushID(index);
                CategoryChip(schemes[index].first, schemes[index].second);
                ImGui::PopID();
            }
        }

        std::string WrapValue(const std::string &value, const float wrapWidth) {
            auto isBreak = [](const char character) {
                return character == ' ' || character == ':' || character == ',' || character == ';';
            };
            std::string wrapped;
            std::string line;
            for (size_t index = 0; index < value.size();) {
                size_t end = index + 1;
                if (!isBreak(value.at(index))) {
                    while (end < value.size() && !isBreak(value.at(end))) {
                        end += 1;
                    }
                    if (end < value.size()) {
                        end += 1;
                    }
                }
                const std::string token = value.substr(index, end - index);
                const std::string candidate = line + token;
                if (line.empty() || ImGui::CalcTextSize(candidate.c_str()).x <= wrapWidth) {
                    line = candidate;
                } else {
                    wrapped += line;
                    wrapped.push_back('\n');
                    line = token;
                }
                index = end;
            }
            wrapped += line;
            return wrapped;
        }

        void DrawWrappedValueBox(const std::string &value, const float width) {
            const ImGuiStyle &style = ImGui::GetStyle();
            const bool empty = value.empty();
            const float wrapWidth = std::max(1.0F, width - (style.FramePadding.x * 2.0F));
            const std::string wrapped = empty ? std::string(UNAVAILABLE) : WrapValue(value, wrapWidth);
            const char *text = wrapped.c_str();
            const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
            const float height = textSize.y + (style.FramePadding.y * 2.0F);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, height));

            ImDrawList *draw = ImGui::GetWindowDrawList();
            const ImVec2 max(origin.x + width, origin.y + height);
            draw->AddRectFilled(origin, max, ImGui::GetColorU32(HexColor(Colors::SURFACE0)), style.FrameRounding);
            draw->AddRect(origin, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
            const ImU32 ink = ImGui::GetColorU32(empty ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            draw->AddText(
                nullptr,
                0.0F,
                ImVec2(origin.x + style.FramePadding.x, origin.y + style.FramePadding.y),
                ink,
                text,
                nullptr,
                wrapWidth
            );
        }

        void DrawCertificateField(const char *label, const std::string &value, const bool copyable) {
            FieldLabel(label);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonSide = ImGui::GetFrameHeight();
            const float fullWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float boxWidth = copyable ? std::max(1.0F, fullWidth - buttonSide - spacing) : fullWidth;
            DrawWrappedValueBox(value, boxWidth);
            if (!copyable) {
                return;
            }
            ImGui::SameLine(0.0F, spacing);
            ImGui::BeginDisabled(value.empty());
            if (PrimaryButton(Icons::COPY, !value.empty(), ImVec2(buttonSide, 0.0F))) {
                ImGui::SetClipboardText(value.c_str());
            }
            if (!value.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", Tr("Copy SHA-256"));
            }
            ImGui::EndDisabled();
        }

        void DrawVerifyDetails(const ApkVerifyResult &result, const bool warnDifferentSignature) {
            if (!result.Started || result.ToolMissing) {
                StatusMessage(StatusMessageTone::Error, Tr("The signature could not be checked."));
                return;
            }
            if (warnDifferentSignature && result.Report.Verified) {
                StatusMessage(
                    StatusMessageTone::Warning,
                    Tr(
                        "This APK is already signed. The new file carries a different signature, "
                        "and the original APK is left unchanged."
                    )
                );
                ImGui::Spacing();
            }
            DrawSchemeChips(result.Report);
            ImGui::Spacing();
            DrawCertificateField(Tr("Certificate DN"), result.Report.SignerDn, false);
            ImGui::Spacing();
            DrawCertificateField(Tr("Certificate SHA-256"), FormatCertificateSha256(result.Report.CertificateSha256), true);
        }

        void DrawCardTitle(const char *title, const bool *verified) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", title);
            if (verified == nullptr) {
                return;
            }
            const char *label = *verified ? Tr("Verified") : Tr("Not Verified");
            const char *icon = *verified ? Icons::CHECK_CIRCLE : Icons::TIMES_CIRCLE;
            const float dpi = GetDpiScale();
            const float messageWidth = (16.0F * dpi) + ImGui::CalcTextSize(icon).x + (6.0F * dpi) + ImGui::CalcTextSize(label).x;
            ImGui::SameLine();
            const float remaining = ImGui::GetContentRegionAvail().x;
            if (remaining > messageWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - messageWidth);
            }
            StatusMessage(*verified ? StatusMessageTone::Positive : StatusMessageTone::Error, label);
        }

        void DrawApkField(Context &context, const std::string &apksigner) {
            auto &work = context.ApkSignerWork;
            FieldLabel(Tr("APK"));
            if (DrawBrowseRow("##ApkPathField", work.ApkPath, Tr("No APK Selected"), "ApkPath", Tr("Browse..."))) {
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
        }

        void DrawAliasSection(Context &context) {
            auto &work = context.ApkSignerWork;
            FieldLabel(Tr("Alias"));
            const bool javaMissing = context.Host.Jdk.JavaHome.empty();
            const bool reading = work.Busy.load() && work.Task == ApkSignerTask::Aliases;
            if (reading) {
                ImGui::TextDisabled("%s", Tr("Reading keystore aliases..."));
            } else if (work.HasAliasResult) {
                if (work.AliasResult.ToolMissing) {
                    if (!javaMissing) {
                        StatusMessage(StatusMessageTone::Error, Tr("keytool was not found in the JDK."));
                    }
                } else if (work.AliasResult.Status != ApkAliasStatus::Ok) {
                    StatusMessage(StatusMessageTone::Error, Tr("The keystore aliases could not be read."));
                } else if (work.Aliases.empty()) {
                    StatusMessage(StatusMessageTone::Warning, Tr("No private-key aliases were found in this keystore."));
                }
            }

            const char *preview = work.SelectedAlias.empty() ? Tr("Select an alias") : work.SelectedAlias.c_str();
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::BeginDisabled(work.Aliases.empty());
            ComboStyle comboStyle;
            if (ImGui::BeginCombo("##Alias", preview)) {
                for (int index = 0; index < static_cast<int>(work.Aliases.size()); ++index) {
                    const std::string &alias = work.Aliases.at(static_cast<std::size_t>(index));
                    ImGui::PushID(index);
                    const bool selected = alias == work.SelectedAlias;
                    if (RoundedSelectable(alias.c_str(), selected)) {
                        if (work.SelectedAlias != alias) {
                            ClearSignResult(work);
                        }
                        work.SelectedAlias = alias;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
        }

        void DrawKeystoreSection(Context &context) {
            auto &work = context.ApkSignerWork;
            bool refreshAliases = false;
            FieldLabel(Tr("Keystore"));
            if (DrawBrowseRow(
                    "##KeystorePathField",
                    work.KeystorePath,
                    Tr("No keystore selected"),
                    "KeystorePath",
                    Tr("Browse...")
                )) {
                const std::string start = work.KeystorePath.empty() ? DirectoryOf(work.ApkPath) : DirectoryOf(work.KeystorePath);
                if (const auto picked = FileDialog::PickFile(
                        Tr("Open Keystore"),
                        Tr("Keystores"),
                        {"*.jks", "*.JKS", "*.keystore", "*.KEYSTORE", "*.p12", "*.P12"},
                        start
                    )) {
                    work.KeystorePath = *picked;
                    ClearSignResult(work);
                    refreshAliases = true;
                }
            }

            ImGui::Spacing();
            FieldLabel(Tr("Keystore Password"));
            ImGui::PushID(static_cast<int>(work.SecretEpoch));
            const SecretEdit storePassword = DrawSecretField(
                "##StorePassword",
                work.StorePassword,
                sizeof work.StorePassword,
                work.ShowStorePassword
            );
            ImGui::PopID();
            if (storePassword.Submitted) {
                work.AliasListScheduled = false;
                refreshAliases = true;
            } else if (storePassword.Edited) {
                work.AliasListDue = std::chrono::steady_clock::now() + std::chrono::milliseconds(1400);
                work.AliasListScheduled = true;
            }
            if (refreshAliases) {
                StartAliasList(context);
            }

            ImGui::Spacing();
            DrawAliasSection(context);

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
            if (!work.KeyPasswordMatches) {
                ImGui::Spacing();
                FieldLabel(Tr("Key Password"));
                ImGui::PushID(static_cast<int>(work.SecretEpoch));
                DrawSecretField("##KeyPassword", work.KeyPassword, sizeof work.KeyPassword, work.ShowKeyPassword);
                ImGui::PopID();
            }
        }

        void DrawOutputField(Context &context) {
            auto &work = context.ApkSignerWork;
            if (SubtitledCheckbox(
                    "##AlignBeforeSigning",
                    &work.Align,
                    Tr("Align before signing"),
                    Tr("Zip-align the APK before the signature is written.")
                )) {
                ClearSignResult(work);
            }

            ImGui::Spacing();
            FieldLabel(Tr("Output"));
            if (DrawBrowseRow("##OutputPathField", work.OutputPath, Tr("No output path"), "OutputPath", Tr("Save As..."))) {
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

        void DrawSignButton(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            const bool signing = work.Busy.load() && work.Task == ApkSignerTask::Sign;
            const float buttonWidth = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            ImGui::PushID("SignApk");
            if (signing) {
                PositiveButton(Tr("Signing..."), true, ImVec2(buttonWidth, 0.0F), true);
            } else if (PositiveButton(Tr("Sign"), CanSign(context, work, apksigner, zipalign), ImVec2(buttonWidth, 0.0F))) {
                StartSign(context, apksigner, zipalign);
            }
            ImGui::PopID();
        }

        void DrawEmptyReport(const char *title, const char *subtitle);

        void DrawFileIdentity(const std::string &path) {
            {
                StyleColor sc;
                sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
                ImGui::TextWrapped("%s", FileNameOf(path).c_str());
            }
            {
                StyleColor sc;
                sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
                ImGui::TextWrapped("%s", path.c_str());
            }
        }

        template<typename Draw>
        void DrawDetailCard(const char *id, const float height, const Draw &draw) {
            StyleColor sc;
            StyleVar sv;
            sc.Push(ImGuiCol_ChildBg, HexColor(Colors::SURFACE1));
            sc.Push(ImGuiCol_Border, HexColor(Colors::SURFACE4));
            sv.Push(ImGuiStyleVar_ChildRounding, 8.0F);
            sv.Push(ImGuiStyleVar_ChildBorderSize, 1.0F);
            sv.Push(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().WindowPadding);
            ImGui::BeginChild(id, ImVec2(0.0F, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
            draw();
            ImGui::EndChild();
        }

        void DrawInputDetail(const ApkSignerWork &work) {
            if (work.ApkPath.empty()) {
                DrawEmptyReport(Tr("No APK Selected"), Tr("Choose an APK to see its signature."));
                return;
            }
            const bool hasStatus = work.HasVerify && work.Verify.Started && !work.Verify.ToolMissing;
            DrawCardTitle(Tr("APK"), hasStatus ? &work.Verify.Report.Verified : nullptr);
            ImGui::Spacing();
            DrawFileIdentity(work.ApkPath);
            ImGui::Spacing();
            if (work.HasVerify) {
                DrawVerifyDetails(work.Verify, true);
            } else if (work.Busy.load() && work.Task == ApkSignerTask::Verify) {
                ImGui::TextDisabled("%s", Tr("Checking the signature..."));
            } else {
                ImGui::TextDisabled("%s", Tr("The signature could not be checked."));
            }
        }

        void DrawOutputDetail(const ApkSignerWork &work) {
            const bool signing = work.Busy.load() && work.Task == ApkSignerTask::Sign;
            if (work.OutputPath.empty() && !signing && !work.HasSignResult) {
                DrawEmptyReport(Tr("No Signed APK"), Tr("Sign an APK to see the result here."));
                return;
            }
            const bool didSign = !signing && work.HasSignResult && work.SignResult.Status == ApkSignStatus::Signed;
            const bool hasStatus = didSign && work.SignResult.Verify.Started && !work.SignResult.Verify.ToolMissing;
            DrawCardTitle(Tr("Signed APK"), hasStatus ? &work.SignResult.Verify.Report.Verified : nullptr);
            ImGui::Spacing();
            DrawFileIdentity(work.OutputPath);
            if (signing) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr("Signing..."));
                return;
            }
            if (!work.HasSignResult) {
                if (!work.OutputPath.empty()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", Tr("Sign the APK to see its signature here."));
                }
                return;
            }

            ImGui::Spacing();
            const ApkSignResult &result = work.SignResult;
            switch (result.Status) {
                case ApkSignStatus::Refused:
                    StatusMessage(StatusMessageTone::Error, RefusalText(result.Refusal));
                    break;
                case ApkSignStatus::AlignFailed:
                    StatusMessage(StatusMessageTone::Error, Tr("Alignment failed."));
                    break;
                case ApkSignStatus::SignFailed:
                    StatusMessage(StatusMessageTone::Error, Tr("Signing failed."));
                    break;
                case ApkSignStatus::Signed:
                    DrawVerifyDetails(result.Verify, false);
                    break;
            }
        }

        void DrawSignerForm(Context &context, const std::string &apksigner, const std::string &zipalign) {
            auto &work = context.ApkSignerWork;
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float columnGap = ImGui::GetStyle().ItemSpacing.x;
            const float columnH = std::max(1.0F, avail.y);
            const float leftW = std::max(1.0F, (avail.x - columnGap) * 0.45F);
            const float rightW = std::max(1.0F, avail.x - columnGap - leftW);

            ImGui::BeginChild("##SignerInputs", ImVec2(leftW, columnH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            ImGui::BeginChild("##SignerFields", ImVec2(0.0F, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None);
            {
                FormDisabled disabled(work.Busy.load());
                DrawApkField(context, apksigner);
                ImGui::Spacing();
                DrawKeystoreSection(context);
                ImGui::Spacing();
                DrawOutputField(context);
                const ApkSignRefusal refusal = SignRefusal(context, work, apksigner, zipalign);
                if (ShowLiveRefusal(refusal, work)) {
                    ImGui::Spacing();
                    StatusMessage(StatusMessageTone::Error, RefusalText(refusal));
                }
            }
            ImGui::EndChild();
            DrawSignButton(context, apksigner, zipalign);
            ImGui::EndChild();

            ImGui::SameLine(0.0F, columnGap);
            ImGui::BeginChild("##SignerReports", ImVec2(rightW, columnH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            const float rowGap = ImGui::GetStyle().ItemSpacing.y;
            const float reportsH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            const float inputCardH = std::max(1.0F, (reportsH - rowGap) * 0.5F);
            const float outputCardH = std::max(1.0F, reportsH - rowGap - inputCardH);
            DrawDetailCard("##InputDetail", inputCardH, [&work] { DrawInputDetail(work); });
            DrawDetailCard("##OutputDetail", outputCardH, [&work] { DrawOutputDetail(work); });
            ImGui::EndChild();
        }

        const char *AdvanceWrappedLine(const char *cursor, const char *end, const float wrapW, const char *&lineEnd) {
            const char *wrap = ImGui::GetFont()->CalcWordWrapPositionA(1.0F, cursor, end, wrapW);
            if (wrap == cursor && cursor + 1 < end) {
                wrap += 1;
            }
            lineEnd = wrap;
            while (lineEnd > cursor && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\n')) {
                lineEnd -= 1;
            }
            while (wrap < end && *wrap == ' ') {
                wrap += 1;
            }
            return wrap;
        }

        int CountWrappedLines(const char *text, const char *end, const float wrapW) {
            int lines = 0;
            for (const char *cursor = text; cursor < end;) {
                lines += 1;
                const char *lineEnd = nullptr;
                cursor = AdvanceWrappedLine(cursor, end, wrapW, lineEnd);
            }
            return lines;
        }

        void DrawCenteredWrappedText(const char *text, const char *end, const float columnX, const float wrapW, float &cursorY) {
            const float textH = ImGui::GetTextLineHeight();
            for (const char *cursor = text; cursor < end;) {
                const char *lineEnd = nullptr;
                const char *next = AdvanceWrappedLine(cursor, end, wrapW, lineEnd);
                const float lineW = ImGui::CalcTextSize(cursor, lineEnd).x;
                ImGui::SetCursorPos(ImVec2(columnX + std::max(0.0F, (wrapW - lineW) * 0.5F), cursorY));
                ImGui::TextUnformatted(cursor, lineEnd);
                cursorY += textH;
                cursor = next;
            }
        }

        void DrawEmptyReport(const char *title, const char *subtitle) {
            const char *titleEnd = title + std::char_traits<char>::length(title);
            const char *subtitleEnd = subtitle + std::char_traits<char>::length(subtitle);
            const float availW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float availH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            const float gap = ImGui::GetStyle().ItemSpacing.y;

            ImGui::SetWindowFontScale(1.35F);
            const float titleH = ImGui::GetTextLineHeight() * static_cast<float>(CountWrappedLines(title, titleEnd, availW));
            ImGui::SetWindowFontScale(1.0F);
            const float subtitleH = ImGui::GetTextLineHeight() * static_cast<float>(CountWrappedLines(subtitle, subtitleEnd, availW));
            const float blockH = titleH + gap + subtitleH;

            const float columnX = ImGui::GetCursorPosX();
            float cursorY = ImGui::GetCursorPosY() + std::max(0.0F, (availH - blockH) * 0.5F);
            ImGui::SetWindowFontScale(1.35F);
            {
                StyleColor sc;
                sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
                DrawCenteredWrappedText(title, titleEnd, columnX, availW, cursorY);
            }
            ImGui::SetWindowFontScale(1.0F);
            cursorY += gap;
            {
                StyleColor sc;
                sc.Push(ImGuiCol_Text, HexColor(Colors::TEXT_MUTED));
                DrawCenteredWrappedText(subtitle, subtitleEnd, columnX, availW, cursorY);
            }
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

        void DrawToolbar(Context &context) {
            if (PrimaryButton(Icons::CHEVRON_LEFT)) {
                LeaveApkSigner(context);
            }
            if (context.UI.Workspace != WorkspacePage::ApkSigner) {
                return;
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
            ImGui::Begin("##ApkSigner", nullptr, APK_SIGNER_WINDOW_FLAGS);
        }

        DrawToolbar(context);
        if (context.UI.Workspace == WorkspacePage::ApkSigner) {
            const std::string &apksigner = context.Host.Sdk.ApksignerPath;
            const std::string &zipalign = context.Host.Sdk.ZipalignPath;
            ImGui::Spacing();
            const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
            ImGui::BeginChild("##ApkSignerBody", ImVec2(0.0F, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            if (apksigner.empty()) {
                DrawMissingSigner(context);
            } else {
                DrawSignerForm(context, apksigner, zipalign);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

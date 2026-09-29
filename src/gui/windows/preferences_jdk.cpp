//
// Created by AbdulMuaz Aqeel on 28/09/2026.
//

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

#include "preferences_jdk.h"
#include "preferences.h"
#include "../application.h"
#include "../theme.h"
#include "../widgets.h"
#include "../../core/jdk.h"
#include "../../core/paths.h"
#include "../../core/sdk.h"
#include "../../core/i18n.h"

namespace CoreDeck {
    namespace {
        constexpr const char *JAVA_DOWNLOAD_URL = "https://www.oracle.com/java/technologies/downloads/#java21";

        struct InstalledJdkCatalog {
            std::future<std::vector<JdkInfo>> Scan;
            std::vector<JdkInfo> Items;
            bool Loaded = false;
        };

        InstalledJdkCatalog &InstalledJdks() {
            static InstalledJdkCatalog catalog;
            return catalog;
        }

        void PollInstalledJdkScan() {
            InstalledJdkCatalog &catalog = InstalledJdks();
            if (!catalog.Scan.valid()) {
                return;
            }
            if (catalog.Scan.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }
            catalog.Items = catalog.Scan.get();
            catalog.Loaded = true;
        }

        bool SameJdkHome(const std::string &left, const std::string &right) {
            if (left == right) {
                return true;
            }
            std::error_code error;
            const std::filesystem::path canonicalLeft = std::filesystem::weakly_canonical(left, error);
            const std::filesystem::path canonicalRight = std::filesystem::weakly_canonical(right, error);
            return !canonicalLeft.empty() && canonicalLeft == canonicalRight;
        }

        std::string JavaRuntimeLabel(const JdkInfo &jdk) {
            const std::string lower = LowerCopy(jdk.VersionString);
            const char *vendor = lower.find("openjdk") != std::string::npos ? Tr("OpenJDK") : Tr("Java");
            if (jdk.MajorVersion > 0) {
                return TrFormat("{0} {1}", vendor, std::to_string(jdk.MajorVersion));
            }
            if (!jdk.VersionString.empty()) {
                return jdk.VersionString;
            }
            return vendor;
        }

        bool JdkChoiceCard(
            const char *id,
            const char *title,
            const char *body,
            const float width,
            const bool selected,
            const bool enabled
        ) {
            const float dpi = GetDpiScale();
            const float pad = 12.0F * dpi;
            const float rounding = 8.0F * dpi;
            const float textWidth = std::max(1.0F, width - (pad * 2.0F));
            const ImVec2 titleSize = ImGui::CalcTextSize(title, nullptr, false, textWidth);
            const ImVec2 bodySize = ImGui::CalcTextSize(body, nullptr, false, textWidth);
            const float gap = 4.0F * dpi;
            const float height = pad + titleSize.y + gap + bodySize.y + pad;

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            if (!enabled) {
                ImGui::BeginDisabled();
            }
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
            const bool hovered = enabled && ImGui::IsItemHovered();
            if (!enabled) {
                ImGui::EndDisabled();
            }
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            const ImVec2 cardMax(origin.x + width, origin.y + height);
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImU32 fill = ImGui::GetColorU32(HexColor(hovered ? Colors::SURFACE2 : Colors::SURFACE1));
            ImU32 border = ImGui::GetColorU32(HexColor(Colors::BORDER));
            float borderPx = 1.0F * dpi;
            if (!enabled) {
                border = ImGui::GetColorU32(HexColor(Colors::BORDER_SUBTLE, 0.7F));
            } else if (selected) {
                border = ImGui::GetColorU32(HexColor(Colors::POSITIVE));
                borderPx = 1.5F * dpi;
            } else if (hovered) {
                border = ImGui::GetColorU32(HexColor(Colors::BORDER_HOVER));
            }
            drawList->AddRectFilled(origin, cardMax, fill, rounding);
            drawList->AddRect(origin, cardMax, border, rounding, 0, borderPx);

            const float textAlpha = enabled ? 1.0F : 0.45F;
            ImVec4 titleColor = HexColor(Colors::TEXT_PRIMARY);
            ImVec4 bodyColor = HexColor(Colors::TEXT_SUBTLE);
            titleColor.w *= textAlpha;
            bodyColor.w *= textAlpha;
            ImFont *font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            drawList->AddText(
                font,
                fontSize,
                ImVec2(origin.x + pad, origin.y + pad),
                ImGui::GetColorU32(titleColor),
                title,
                nullptr,
                textWidth
            );
            drawList->AddText(
                font,
                fontSize,
                ImVec2(origin.x + pad, origin.y + pad + titleSize.y + gap),
                ImGui::GetColorU32(bodyColor),
                body,
                nullptr,
                textWidth
            );
            return enabled && pressed;
        }

        struct AvdCatalogSnapshot {
            std::vector<std::string> Names;
            std::vector<AvdInfo> Avds;
            std::unordered_map<std::string, std::vector<EmulatorOption>> Options;
        };

        struct AvdCatalogRefresh {
            std::future<AvdCatalogSnapshot> Future;
            std::optional<SdkInfo> Pending;
        };

        AvdCatalogRefresh &CatalogRefresh() {
            static AvdCatalogRefresh job;
            return job;
        }

        AvdCatalogSnapshot LoadAvdCatalog(const SdkInfo &sdk) {
            AvdCatalogSnapshot snapshot;
            snapshot.Names = ListAvdNames(sdk);
            snapshot.Avds = LoadAvds(snapshot.Names);
            snapshot.Options.reserve(snapshot.Names.size());
            for (const std::string &avdName: snapshot.Names) {
                snapshot.Options.emplace(avdName, LoadOptionsFromFile(GetOptionsConfigPath(avdName)));
            }
            return snapshot;
        }

        void StartAvdCatalogRefresh(AvdCatalogRefresh &job, const SdkInfo &sdk) {
            job.Future = std::async(std::launch::async, [sdk] {
                return LoadAvdCatalog(sdk);
            });
        }

        void BeginAvdCatalogRefresh(const SdkInfo &sdk) {
            AvdCatalogRefresh &job = CatalogRefresh();
            if (job.Future.valid()) {
                job.Pending = sdk;
                return;
            }
            StartAvdCatalogRefresh(job, sdk);
        }

        void ApplyAvdCatalogSnapshot(Context &context, AvdCatalogSnapshot snapshot) {
            context.Catalog.AvdNames = std::move(snapshot.Names);
            context.Catalog.Avds = std::move(snapshot.Avds);
            for (const std::string &avdName: context.Catalog.AvdNames) {
                context.Catalog.PerAvdOptions[avdName] = std::move(snapshot.Options[avdName]);
            }

            context.DiskUsage.PerAvdCache.clear();
            if (!context.DiskUsage.Loading.load()) {
                context.DiskUsage.LastScan = {};
                context.DiskUsage.Ready = false;
            }

            if (!context.Catalog.Avds.empty()) {
                context.Catalog.SelectedAvd = 0;
            } else {
                context.Catalog.SelectedAvd = -1;
            }
            context.Catalog.PreviousSelectedAvd = -1;
        }

        void PollAvdCatalogRefresh(Context &context) {
            AvdCatalogRefresh &job = CatalogRefresh();
            if (!job.Future.valid()) {
                return;
            }
            if (job.Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }

            ApplyAvdCatalogSnapshot(context, job.Future.get());
            if (!job.Pending.has_value()) {
                return;
            }

            const SdkInfo sdk = std::move(*job.Pending);
            job.Pending.reset();
            StartAvdCatalogRefresh(job, sdk);
        }

        void UseListedJdk(Context &context, const JdkInfo &jdk, char *jdkPathBuffer, const size_t bufferSize) {
            if (jdk.JavaHome.empty() || jdkPathBuffer == nullptr || bufferSize == 0) {
                return;
            }
            const bool alreadyApplied =
                context.Host.Jdk.Source == JdkSource::Override && SameJdkHome(jdk.JavaHome, context.Host.Jdk.JavaHome);
            strncpy(jdkPathBuffer, jdk.JavaHome.c_str(), bufferSize - 1);
            jdkPathBuffer[bufferSize - 1] = '\0';
            if (alreadyApplied) {
                return;
            }

            Paths::Onboarding::SaveJdkPathOverride(jdk.JavaHome);
            context.Host.Jdk = jdk;
            context.Host.Jdk.Source = JdkSource::Override;
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            BeginAvdCatalogRefresh(context.Host.Sdk);
            context.UI.HideHealthCheckBanner = false;
        }

        void DrawInstalledJdkList(Context &context, char *jdkPathBuffer, const size_t bufferSize) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
#ifdef _WIN32
            ImGui::TextUnformatted(Tr("Installed on this Windows"));
#elif defined(__APPLE__)
            ImGui::TextUnformatted(Tr("Installed on this macOS"));
#elif defined(__linux__)
            ImGui::TextUnformatted(Tr("Installed on this Linux"));
#else
            ImGui::TextUnformatted(Tr("Installed on this Unix-like system"));
#endif
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", TrFormat("Select a JDK {0} or newer.", std::to_string(JDK_MINIMUM_MAJOR)).c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const InstalledJdkCatalog &catalog = InstalledJdks();
            if (catalog.Items.empty()) {
                if (!catalog.Loaded) {
                    ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
                    ImGui::TextUnformatted(Tr("Looking for installed JDKs..."));
                    ImGui::PopStyleColor();
                } else {
                    const std::string message = TrFormat(
                        "No JDK was found. Install JDK {0} separately, or choose a folder below.",
                        std::to_string(JDK_RECOMMENDED_MAJOR)
                    );
                    StatusMessage(StatusMessageTone::Error, message.c_str());
                    if (ImGui::TextLink(Tr("Download Java..."))) {
                        OpenUrl(JAVA_DOWNLOAD_URL);
                    }
                }
                return;
            }

            const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float gap = 4.0F * GetDpiScale();
            for (size_t index = 0; index < catalog.Items.size(); ++index) {
                const JdkInfo &jdk = catalog.Items.at(index);
                if (index > 0) {
                    ImGui::Dummy(ImVec2(0.0F, gap));
                }
                const bool selected = jdk.IsValid && SameJdkHome(jdk.JavaHome, context.Host.Jdk.JavaHome);
                const std::string title = JavaRuntimeLabel(jdk);
                const std::string body = jdk.IsValid
                                             ? jdk.JavaHome
                                             : TrFormat("Older than JDK {0}. {1}", std::to_string(JDK_MINIMUM_MAJOR), jdk.JavaHome);
                const std::string id = StrConcat("##prefs_jdk_", std::to_string(index));
                if (JdkChoiceCard(id.c_str(), title.c_str(), body.c_str(), width, selected, jdk.IsValid)) {
                    UseListedJdk(context, jdk, jdkPathBuffer, bufferSize);
                }
            }
        }

        const char *JdkSourceLabel(const JdkSource source) {
            switch (source) {
                case JdkSource::Override:
                    return Tr("Custom Path");
                case JdkSource::JavaHomeEnv:
                    return "JAVA_HOME";
                case JdkSource::Detected:
                    return Tr("Auto-Detected");
                case JdkSource::None:
                default:
                    return Tr("None");
            }
        }

        void DrawJdkStatus(const JdkInfo &jdk) {
            if (!jdk.IsFound) {
                StatusMessage(StatusMessageTone::Error, Tr("No JDK found at this location."));
                return;
            }

            const char *version = jdk.VersionString.empty() ? Tr("Java") : jdk.VersionString.c_str();
            if (jdk.IsValid) {
                const std::string message = TrFormat("{0} ({1})", version, JdkSourceLabel(jdk.Source));
                StatusMessage(StatusMessageTone::Positive, message.c_str());
            } else {
                const std::string message = TrFormat(
                    "{0} ({1}) - requires JDK {2} or newer.",
                    version,
                    JdkSourceLabel(jdk.Source),
                    std::to_string(JDK_MINIMUM_MAJOR)
                );
                StatusMessage(StatusMessageTone::Error, message.c_str());
            }
        }
    }

    void RequestPreferencesJdkScan() {
        InstalledJdkCatalog &catalog = InstalledJdks();
        if (catalog.Scan.valid()) {
            return;
        }
        catalog.Loaded = false;
        catalog.Scan = std::async(std::launch::async, [] {
            return ListInstalledJdks();
        });
    }

    void PollPreferencesJdkWork(Context &context) {
        PollInstalledJdkScan();
        PollAvdCatalogRefresh(context);
    }

    void DrawPreferencesJdkSection(Context &context, char *jdkPathBuffer, const size_t bufferSize) {
        PreferencesSectionHeader(
            Tr("Java (JDK)"),
            Tr("The Android command-line tools (avdmanager, sdkmanager) run on Java and require JDK 17 or newer. Point CoreDeck at a compatible JDK if your system default is older.")
        );

        ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_PRIMARY));
        ImGui::TextUnformatted(Tr("Currently used"));
        ImGui::PopStyleColor();
        if (context.Host.Jdk.IsFound) {
            DrawJdkStatus(context.Host.Jdk);
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextWrapped("%s", context.Host.Jdk.JavaHome.c_str());
            ImGui::PopStyleColor();
        } else {
            StatusMessage(
                StatusMessageTone::Warning,
                Tr("No JDK detected. The command-line tools will use whatever 'java' is on your PATH.")
            );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawInstalledJdkList(context, jdkPathBuffer, bufferSize);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const std::string pathStr = PathPicker(
            "##JdkPrefs",
            Tr("JDK home"),
            Tr("Path to a JDK home directory"),
            Tr("Select JDK home directory"),
            jdkPathBuffer,
            bufferSize
        );
        const bool binExists = !pathStr.empty() &&
                               (std::filesystem::exists(
                                    Paths::JoinPaths({pathStr, "bin", "java" + Paths::GetExecutableExtension()})
                                ) ||
                                std::filesystem::exists(
                                    Paths::JoinPaths({pathStr, "Contents", "Home", "bin", "java" + Paths::GetExecutableExtension()})
                                ));

        const bool matchesActive =
            context.Host.Jdk.IsFound && SameJdkHome(pathStr, context.Host.Jdk.JavaHome);
        if (pathStr.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextUnformatted(Tr("Leave empty to auto-detect from JAVA_HOME or standard install paths."));
            ImGui::PopStyleColor();
        } else if (!binExists) {
            StatusMessage(StatusMessageTone::Error, Tr("No 'bin/java' found in this directory."));
        } else if (matchesActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextUnformatted(Tr("This is the JDK CoreDeck is using."));
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::TEXT_SUBTLE));
            ImGui::TextUnformatted(Tr("Click Apply to validate the Java version and use this JDK."));
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (PrimaryButton(Tr("Apply JDK Path"), binExists)) {
            Paths::Onboarding::SaveJdkPathOverride(pathStr);
            context.Host.Jdk = DetectJdk();
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            RefreshAvds(context);
            context.UI.HideHealthCheckBanner = false;
            strncpy(jdkPathBuffer, context.Host.Jdk.JavaHome.c_str(), bufferSize - 1);
            jdkPathBuffer[bufferSize - 1] = '\0';
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !binExists) {
            ImGui::SetTooltip("%s", Tr("Choose a directory that contains bin/java before applying."));
        }

        ImGui::SameLine();
        if (PrimaryButton(Tr("Use Default Discovery"), true)) {
            Paths::Onboarding::ClearJdkPathOverride();
            context.Host.Jdk = DetectJdk();
            ApplyJdkToSdk(context.Host.Sdk, context.Host.Jdk);
            context.Host.Manager.SetSdk(context.Host.Sdk);
            RefreshAvds(context);
            context.UI.HideHealthCheckBanner = false;
            strncpy(jdkPathBuffer, context.Host.Jdk.JavaHome.c_str(), bufferSize - 1);
            jdkPathBuffer[bufferSize - 1] = '\0';
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Tr("Forget the saved JDK and detect it from JAVA_HOME / standard paths."));
        }
    }
}

//
// Created by AbdulMuaz Aqeel on 19/04/2026.
//

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <filesystem>
#include <optional>
#include <sstream>

#include "system_image.h"
#include "paths.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        // Java's PrintStream buffers 8192 characters and auto-flushes on newline.
        // A wider COLUMNS makes each Android CLI progress redraw spill that buffer.
        constexpr const char *SDK_MANAGER_PROGRESS_COLUMNS = "16384";

        std::string TrimProgressText(const std::string &text) {
            const auto start = text.find_first_not_of(" \t");
            if (start == std::string::npos) {
                return {};
            }
            const auto end = text.find_last_not_of(" \t");
            return text.substr(start, end - start + 1);
        }

        std::string FractionAfterPercent(const std::string &line, const size_t pctPos) {
            const auto open = line.find('(', pctPos);
            if (open == std::string::npos) {
                return {};
            }
            const auto close = line.find(')', open);
            if (close == std::string::npos || close <= open + 1) {
                return {};
            }

            const std::string inner = TrimProgressText(line.substr(open + 1, close - open - 1));
            const auto slash = inner.find('/');
            if (slash == std::string::npos) {
                return inner;
            }
            return TrimProgressText(inner.substr(0, slash)) + " / " + TrimProgressText(inner.substr(slash + 1));
        }

        std::string StatusForProgress(const std::string &line, const size_t pctPos) {
            const std::string fraction = FractionAfterPercent(line, pctPos);
            const bool unzipping = line.find("Unzipping") != std::string::npos;
            if (!fraction.empty()) {
                return (unzipping ? "Unzipping " : "Downloading ") + fraction;
            }

            std::string after;
            if (pctPos + 1 < line.size()) {
                after = TrimProgressText(line.substr(pctPos + 1));
            }
            if (!after.empty() && !after.starts_with("ETA:")) {
                return after;
            }
            if (unzipping) {
                return "Unzipping...";
            }
            return "Downloading...";
        }

        bool IsProgressPercent(const std::string &line, const size_t pctPos) {
            if (pctPos == 0 || line[pctPos] != '%') {
                return false;
            }
            const char after = pctPos + 1 < line.size() ? line[pctPos + 1] : '\0';
            if (after != '\0' && after != ' ' && after != '\t' && after != '(') {
                return false;
            }

            size_t start = pctPos;
            while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9') {
                --start;
            }
            if (start == pctPos || pctPos - start > 3) {
                return false;
            }
            return true;
        }

        void ParseProgressLine(const std::string &line, const std::shared_ptr<InstallProgressData> &progress) {
            if (!progress) {
                return;
            }

            const SdkManagerProgressLine parsed = ParseSdkManagerProgressLine(line);
            std::lock_guard lock(progress->Mutex);
            if (parsed.HasPercent) {
                progress->Percent = static_cast<float>(parsed.Percent) / 100.0F;
                if (!parsed.Status.empty()) {
                    progress->StatusText = parsed.Status;
                }
                progress->DetailText = parsed.Status.empty() ? line : parsed.Status;
                return;
            }

            if (!line.empty() && line.size() <= 512) {
                progress->DetailText = line;
            }
        }

        std::string CanonicalSystemImagePackagePath(std::string packagePath) {
            if (packagePath.starts_with("system-images/")) {
                std::ranges::replace(packagePath, '/', ';');
            }
            return packagePath;
        }

        bool IsAvdManagerDiagnostic(const std::string &line) {
            return line.starts_with("Error:") || line.starts_with("Warning:");
        }
    }

    std::optional<RemoteSystemImage> ParseRemoteSystemImageLine(const std::string &rawLine) {
        std::string line = rawLine;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            return std::nullopt;
        }
        line = line.substr(start);

        if (!line.starts_with("system-images;") && !line.starts_with("system-images/")) {
            return std::nullopt;
        }

        const auto tokenEnd = line.find_first_of(" \t|");
        std::string packagePath = CanonicalSystemImagePackagePath(
            tokenEnd == std::string::npos ? line : line.substr(0, tokenEnd)
        );

        std::vector<std::string> parts;
        std::istringstream partStream(packagePath);
        std::string part;
        while (std::getline(partStream, part, ';')) {
            parts.push_back(part);
        }
        if (parts.size() < 4 || !parts[1].starts_with("android-")) {
            return std::nullopt;
        }

        RemoteSystemImage img;
        img.PackagePath = std::move(packagePath);
        img.ApiLevel = parts[1].substr(8);
        img.Variant = parts[2];
        img.Abi = parts[3];
        img.DisplayName = StrConcat("Android ", img.ApiLevel, " (", img.Variant, ", ", img.Abi, ")");
        return img;
    }

    DeviceProfileList ParseAvdManagerDeviceList(const std::string &output) {
        DeviceProfileList result;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }
            if (IsAvdManagerDiagnostic(line)) {
                if (line.find("Could not load devices") != std::string::npos) {
                    result.SkippedDeviceDefinitions = true;
                }
                continue;
            }

            DeviceProfile device;
            device.Id = line;
            device.Name = line;
            std::ranges::replace(device.Name, '_', ' ');
            if (!device.Name.empty()) {
                device.Name[0] = static_cast<char>(std::toupper(device.Name[0]));
            }
            result.Profiles.push_back(std::move(device));
        }

        return result;
    }

    DeviceProfileList ListDeviceProfiles(const SdkInfo &sdk) {
        if (sdk.AvdManagerPath.empty()) {
            return {};
        }

        const std::string output = RunCommandArgs(sdk.AvdManagerPath, {"list", "device", "-c"}, "", sdk.ToolEnv);
        return ParseAvdManagerDeviceList(output);
    }

    std::vector<SystemImage> ListSystemImages(const SdkInfo &sdk) {
        std::vector<SystemImage> images;
        if (sdk.SdkPath.empty()) {
            return images;
        }

        const std::string sysImgRoot = Paths::JoinPaths({sdk.SdkPath, "system-images"});
        if (!std::filesystem::exists(sysImgRoot)) {
            return images;
        }

        // Structure: system-images/android-XX/variant/abi/
        for (const auto &apiEntry: std::filesystem::directory_iterator(sysImgRoot)) {
            if (!apiEntry.is_directory()) {
                continue;
            }
            const std::string apiDirName = apiEntry.path().filename().string();

            std::string apiLevel;
            if (apiDirName.starts_with("android-")) {
                apiLevel = apiDirName.substr(8);
            } else {
                continue;
            }

            for (const auto &variantEntry: std::filesystem::directory_iterator(apiEntry.path())) {
                if (!variantEntry.is_directory()) {
                    continue;
                }
                const std::string variant = variantEntry.path().filename().string();

                for (const auto &abiEntry: std::filesystem::directory_iterator(variantEntry.path())) {
                    if (!abiEntry.is_directory()) {
                        continue;
                    }
                    const std::string abi = abiEntry.path().filename().string();

                    const std::string sysImg = Paths::JoinPaths({abiEntry.path().string(), "system.img"});
                    if (!std::filesystem::exists(sysImg)) {
                        continue;
                    }

                    SystemImage img;
                    img.ApiLevel = apiLevel;
                    img.Variant = variant;
                    img.Abi = abi;
                    img.PackagePath = StrConcat("system-images;", apiDirName, ";", variant, ";", abi);
                    img.DisplayName = StrConcat("Android ", apiLevel, " (", variant, ", ", abi, ")");
                    images.push_back(img);
                }
            }
        }

        // Sort by API level descending (newest first)
        std::ranges::sort(images, [](const SystemImage &a, const SystemImage &b) {
            const int apiA = static_cast<int>(std::strtol(a.ApiLevel.c_str(), nullptr, 10));
            const int apiB = static_cast<int>(std::strtol(b.ApiLevel.c_str(), nullptr, 10));
            return apiA > apiB;
        });

        return images;
    }

    std::vector<RemoteSystemImage> ListRemoteSystemImages(
        const SdkInfo &sdk,
        const std::vector<SystemImage> &installedImages
    ) {
        std::vector<RemoteSystemImage> results;
        if (sdk.SdkManagerPath.empty()) {
            return results;
        }

        const std::string output = RunCommandArgs(sdk.SdkManagerPath, {"--list"}, "", sdk.ToolEnv);

        std::unordered_set<std::string> installedSet;
        for (const auto &img: installedImages) {
            installedSet.insert(CanonicalSystemImagePackagePath(img.PackagePath));
        }

        std::unordered_set<std::string> seenPackages;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            auto parsed = ParseRemoteSystemImageLine(line);
            if (!parsed.has_value()) {
                continue;
            }
            if (!seenPackages.insert(parsed->PackagePath).second) {
                continue;
            }

            parsed->IsInstalled = installedSet.contains(parsed->PackagePath);
            results.push_back(std::move(*parsed));
        }

        std::ranges::sort(results, [](const RemoteSystemImage &a, const RemoteSystemImage &b) {
            const int apiA = static_cast<int>(std::strtol(a.ApiLevel.c_str(), nullptr, 10));
            const int apiB = static_cast<int>(std::strtol(b.ApiLevel.c_str(), nullptr, 10));
            if (apiA != apiB) {
                return apiA > apiB;
            }
            if (a.IsInstalled != b.IsInstalled) {
                return a.IsInstalled;
            }
            return a.DisplayName < b.DisplayName;
        });

        return results;
    }

    SdkManagerProgressLine ParseSdkManagerProgressLine(const std::string &line) {
        SdkManagerProgressLine result;
        size_t pctPos = line.rfind('%');
        while (pctPos != std::string::npos) {
            if (IsProgressPercent(line, pctPos)) {
                size_t start = pctPos;
                while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9') {
                    --start;
                }
                const int pct = static_cast<int>(std::strtol(line.c_str() + start, nullptr, 10));
                if (pct >= 0 && pct <= 100) {
                    result.HasPercent = true;
                    result.Percent = pct;
                    result.Status = StatusForProgress(line, pctPos);
                    return result;
                }
            }
            if (pctPos == 0) {
                break;
            }
            pctPos = line.rfind('%', pctPos - 1);
        }
        return result;
    }

    EnvVars SdkManagerInstallEnvironment(EnvVars env) {
        for (auto &var: env) {
            if (var.Name == "COLUMNS") {
                var.Value = SDK_MANAGER_PROGRESS_COLUMNS;
                return env;
            }
        }
        env.insert(env.begin(), {.Name = "COLUMNS", .Value = SDK_MANAGER_PROGRESS_COLUMNS});
        return env;
    }

    bool InstallSystemImage(
        const SdkInfo &sdk,
        const std::string &packagePath,
        const std::shared_ptr<InstallProgressData> &progress
    ) {
        if (sdk.SdkManagerPath.empty() || packagePath.empty()) {
            return false;
        }

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->StatusText = "Starting download...";
            progress->Percent = 0.0F;
        }

        // Semicolon ids. Older sdkmanager requires them. cmdline-tools 23 accepts
        // them and rewrites '/' internally, so one form serves both.
        StreamCommandArgs(
            sdk.SdkManagerPath,
            {"--install", packagePath},
            "",
            [&progress](const std::string &line) {
                ParseProgressLine(line, progress);
            },
            SdkManagerInstallEnvironment(sdk.ToolEnv)
        );

        // Verify
        std::string fsPath = packagePath;
        std::ranges::replace(fsPath, ';', '/');
        const std::string sysImg = Paths::JoinPaths({sdk.SdkPath, fsPath, "system.img"});
        const bool ok = std::filesystem::exists(sysImg);

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->Finished = true;
            progress->Succeeded = ok;
            progress->Percent = ok ? 1.0F : progress->Percent;
            progress->StatusText = ok ? "Installation Completed!" : "Installation Failed!";
        }

        return ok;
    }

    bool UninstallSystemImage(const SdkInfo &sdk, const std::string &packagePath) {
        if (sdk.SdkManagerPath.empty() || packagePath.empty()) {
            return false;
        }

        RunCommandArgs(sdk.SdkManagerPath, {"--uninstall", packagePath}, "y\n", sdk.ToolEnv);

        std::string fsPath = packagePath;
        std::ranges::replace(fsPath, ';', '/');
        const std::string sysImg = Paths::JoinPaths({sdk.SdkPath, fsPath, "system.img"});
        return !std::filesystem::exists(sysImg);
    }

    LicenseStatus InterpretSdkLicenseOutput(const std::string &output) {
        if (output.find("All SDK package licenses accepted") != std::string::npos) {
            return LicenseStatus::AllAccepted;
        }
        if (output.find("The --licenses option is no longer needed") != std::string::npos) {
            return LicenseStatus::AllAccepted;
        }
        if (output.find("licenses not accepted") != std::string::npos) {
            return LicenseStatus::SomeUnaccepted;
        }
        return LicenseStatus::CheckFailed;
    }

    LicenseStatus CheckSdkLicenses(const SdkInfo &sdk) {
        if (sdk.SdkManagerPath.empty()) {
            return LicenseStatus::CheckFailed;
        }

        const std::string output = RunCommandArgs(sdk.SdkManagerPath, {"--licenses"}, "N\n", sdk.ToolEnv);
        return InterpretSdkLicenseOutput(output);
    }

    bool AcceptSdkLicenses(const SdkInfo &sdk) {
        if (sdk.SdkManagerPath.empty()) {
            return false;
        }

        std::string yes;
        yes.reserve(static_cast<size_t>(64 * 2));
        for (int i = 0; i < 64; ++i) {
            yes += "y\n";
        }

        const std::string output = RunCommandArgs(sdk.SdkManagerPath, {"--licenses"}, yes, sdk.ToolEnv);
        return InterpretSdkLicenseOutput(output) == LicenseStatus::AllAccepted;
    }
}

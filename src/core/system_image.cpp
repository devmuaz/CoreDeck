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
#include "sdk_manager.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        std::string CanonicalSystemImagePackagePath(std::string packagePath) {
            if (packagePath.starts_with("system-images/")) {
                std::ranges::replace(packagePath, '/', ';');
            }
            return packagePath;
        }

        void ApplyProgressLine(
            const SdkManagerProgressLine &parsed,
            const std::string &raw,
            const std::shared_ptr<InstallProgressData> &progress
        ) {
            if (!progress) {
                return;
            }

            std::lock_guard lock(progress->Mutex);
            if (parsed.HasPercent) {
                progress->Percent = static_cast<float>(parsed.Percent) / 100.0F;
                if (!parsed.Status.empty()) {
                    progress->StatusText = parsed.Status;
                }
                progress->DetailText = parsed.Status.empty() ? raw : parsed.Status;
                return;
            }

            if (!raw.empty() && raw.size() <= 512) {
                progress->DetailText = raw;
            }
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
        const auto output = RunSdkManager(sdk, {"--list"});
        if (!output) {
            return results;
        }

        std::unordered_set<std::string> installedSet;
        for (const auto &img: installedImages) {
            installedSet.insert(CanonicalSystemImagePackagePath(img.PackagePath));
        }

        std::unordered_set<std::string> seenPackages;
        std::istringstream stream(*output);
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

    bool InstallSystemImage(
        const SdkInfo &sdk,
        const std::string &packagePath,
        const std::shared_ptr<InstallProgressData> &progress
    ) {
        if (packagePath.empty()) {
            return false;
        }

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->StatusText = "Starting download...";
            progress->Percent = 0.0F;
        }

        // Semicolon ids. Older sdkmanager requires them. cmdline-tools 23 accepts
        // them and rewrites '/' internally, so one form serves both.
        if (!RunSdkManagerInstall(
                sdk,
                {"--install", packagePath},
                "",
                [&progress](const SdkManagerProgressLine &parsed, const std::string &raw) {
                    ApplyProgressLine(parsed, raw, progress);
                }
            )) {
            if (progress) {
                std::lock_guard lock(progress->Mutex);
                progress->Finished = true;
                progress->Succeeded = false;
                progress->StatusText = "Installation Failed!";
            }
            return false;
        }

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
        if (packagePath.empty()) {
            return false;
        }

        if (!RunSdkManager(sdk, {"--uninstall", packagePath}, "y\n")) {
            return false;
        }

        std::string fsPath = packagePath;
        std::ranges::replace(fsPath, ';', '/');
        const std::string sysImg = Paths::JoinPaths({sdk.SdkPath, fsPath, "system.img"});
        return !std::filesystem::exists(sysImg);
    }
}

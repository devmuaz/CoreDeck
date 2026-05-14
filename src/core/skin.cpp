//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

#include "skin.h"
#include "utilities.h"

#include <ranges>

namespace CoreDeck {
    namespace Fs = std::filesystem;

    namespace {
        std::string PrettifyName(const std::string &id) {
            std::string out = id;
            std::ranges::replace(out, '_', ' ');
            bool atStart = true;
            for (auto &c: out) {
                if (atStart && std::isalpha(static_cast<unsigned char>(c))) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    atStart = false;
                } else if (c == ' ') {
                    atStart = true;
                }
            }
            return out;
        }

        bool IsSkinDirectory(const Fs::path &dir) {
            std::error_code ec;
            if (!Fs::is_directory(dir, ec)) {
                return false;
            }
            return Fs::exists(dir / "layout", ec);
        }

        void CollectSkinsFrom(const Fs::path &root, const SkinSource &source, std::vector<Skin> &out) {
            std::error_code ec;
            if (!Fs::is_directory(root, ec)) {
                return;
            }

            for (const auto &entry: Fs::directory_iterator(root, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_directory(ec)) {
                    continue;
                }
                const auto &dir = entry.path();
                if (!IsSkinDirectory(dir)) {
                    continue;
                }

                std::string dirName = dir.filename().string();
                out.emplace_back(
                    dirName,
                    PrettifyName(dirName),
                    dir.string(),
                    source
                );
            }
        }

        int SourcePriority(const SkinSource &source) {
            switch (source) {
                case SkinSource::Sdk:
                    return 0;
                case SkinSource::SystemImage:
                    return 1;
                case SkinSource::Platform:
                    return 2;
            }
            return 99;
        }
    }

    std::vector<Skin> ListSkins(const SdkInfo &sdk) {
        std::vector<Skin> all;
        if (sdk.SdkPath.empty()) {
            return all;
        }

        CollectSkinsFrom(Fs::path(sdk.SdkPath) / "skins", SkinSource::Sdk, all);

        const Fs::path sysImgRoot = Fs::path(sdk.SdkPath) / "system-images";
        std::error_code ec;
        if (Fs::is_directory(sysImgRoot, ec)) {
            for (const auto &api: Fs::directory_iterator(sysImgRoot, ec)) {
                if (!api.is_directory(ec)) {
                    continue;
                }
                for (const auto &variant: Fs::directory_iterator(api.path(), ec)) {
                    if (!variant.is_directory(ec)) {
                        continue;
                    }
                    for (const auto &abi: Fs::directory_iterator(variant.path(), ec)) {
                        if (!abi.is_directory(ec)) {
                            continue;
                        }
                        CollectSkinsFrom(abi.path() / "skins", SkinSource::SystemImage, all);
                    }
                }
            }
        }

        const Fs::path platformsRoot = Fs::path(sdk.SdkPath) / "platforms";
        if (Fs::is_directory(platformsRoot, ec)) {
            for (const auto &platform: Fs::directory_iterator(platformsRoot, ec)) {
                if (!platform.is_directory(ec)) {
                    continue;
                }
                CollectSkinsFrom(platform.path() / "skins", SkinSource::Platform, all);
            }
        }

        std::unordered_map<std::string, size_t> bestByName;
        for (size_t i = 0; i < all.size(); ++i) {
            const std::string key = LowerCopy(all[i].Name);
            auto it = bestByName.find(key);
            if (it == bestByName.end()) {
                bestByName.emplace(key, i);
            } else if (SourcePriority(all[i].Source) < SourcePriority(all[it->second].Source)) {
                it->second = i;
            }
        }

        std::vector<Skin> deduped;
        deduped.reserve(bestByName.size());
        for (auto &idx: bestByName | std::views::values) {
            deduped.push_back(std::move(all[idx]));
        }

        std::ranges::sort(deduped, [](const Skin &a, const Skin &b) {
            return LowerCopy(a.DisplayName) < LowerCopy(b.DisplayName);
        });

        return deduped;
    }

    std::optional<Skin> FindSkinForDevice(const std::vector<Skin> &skins, const std::string &deviceId) {
        if (deviceId.empty() || skins.empty()) {
            return std::nullopt;
        }

        const std::string needle = LowerCopy(deviceId);

        for (const auto &s: skins) {
            if (LowerCopy(s.Name) == needle) {
                return s;
            }
        }
        for (const auto &s: skins) {
            const std::string lower = LowerCopy(s.Name);
            if (lower.find(needle) != std::string::npos || needle.find(lower) != std::string::npos) {
                return s;
            }
        }
        return std::nullopt;
    }

    const char *SkinSourceLabel(const SkinSource &source) {
        switch (source) {
            case SkinSource::Sdk:
                return "SDK";
            case SkinSource::SystemImage:
                return "System Image";
            case SkinSource::Platform:
                return "Platform";
        }
        return "";
    }
}
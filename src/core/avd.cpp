//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "avd.h"
#include "paths.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    static std::unordered_map<std::string, std::string> ParseConfigFile(const std::string &path) {
        std::unordered_map<std::string, std::string> config;
        std::ifstream file(path);
        if (!file.is_open()) return config;

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            auto key = line.substr(0, eq);
            auto value = line.substr(eq + 1);

            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());

            config[key] = value;
        }

        return config;
    }

    static std::vector<std::string> SplitConfigList(const std::string &value) {
        std::vector<std::string> items;
        std::stringstream stream(value);
        std::string item;
        while (std::getline(stream, item, ',')) {
            while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
            while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(item.begin());
            if (!item.empty()) items.push_back(item);
        }
        return items;
    }

    static bool HasTag(const std::vector<std::string> &tags, const std::string &needle) {
        return std::ranges::any_of(tags, [&](const std::string &tag) {
            return LowerCopy(tag) == needle;
        });
    }

    static void ExtractSystemImageInfo(AvdInfo &avd, const std::unordered_map<std::string, std::string> &config) {
        if (const auto it = config.find("image.sysdir.1"); it != config.end()) {
            avd.SystemImagePath = it->second;

            std::string sysdir = it->second;
            std::ranges::replace(sysdir, '\\', '/');

            if (auto start = sysdir.find("android-"); start != std::string::npos) {
                start += 8;
                if (const auto end = sysdir.find('/', start); end != std::string::npos) {
                    avd.ApiLevel = sysdir.substr(start, end - start);

                    const auto variantStart = end + 1;
                    if (const auto variantEnd = sysdir.find('/', variantStart); variantEnd != std::string::npos) {
                        avd.SystemImageVariant = sysdir.substr(variantStart, variantEnd - variantStart);

                        const auto abiStart = variantEnd + 1;
                        if (const auto abiEnd = sysdir.find('/', abiStart); abiEnd != std::string::npos && avd.Abi.empty()) {
                            avd.Abi = sysdir.substr(abiStart, abiEnd - abiStart);
                        }
                    }
                }
            }
        }

        if (const auto it = config.find("tag.id"); it != config.end()) {
            avd.SystemImageTagId = it->second;
        }
        if (const auto it = config.find("tag.display"); it != config.end()) {
            avd.SystemImageTagDisplay = it->second;
        }
        if (const auto it = config.find("tag.ids"); it != config.end()) {
            avd.SystemImageTagIds = SplitConfigList(it->second);
        } else if (!avd.SystemImageTagId.empty()) {
            avd.SystemImageTagIds = {avd.SystemImageTagId};
        }
        if (const auto it = config.find("tag.displaynames"); it != config.end()) {
            avd.SystemImageTagDisplayNames = SplitConfigList(it->second);
        } else if (!avd.SystemImageTagDisplay.empty()) {
            avd.SystemImageTagDisplayNames = {avd.SystemImageTagDisplay};
        }

        const std::string variant = LowerCopy(avd.SystemImageVariant);
        const std::string tagId = LowerCopy(avd.SystemImageTagId);
        const std::string tagDisplay = LowerCopy(avd.SystemImageTagDisplay);
        const std::string tagDisplayNames = LowerCopy(StrConcat(
            avd.SystemImageTagDisplay,
            " ",
            config.contains("tag.displaynames") ? config.at("tag.displaynames") : ""
        ));

        avd.IsGooglePlayImage = variant.find("google_apis_playstore") != std::string::npos ||
                                tagId == "google_apis_playstore" ||
                                HasTag(avd.SystemImageTagIds, "google_apis_playstore") ||
                                tagDisplay.find("play") != std::string::npos;

        avd.IsGoogleApisImage = avd.IsGooglePlayImage ||
                                variant.find("google_apis") != std::string::npos ||
                                tagId == "google_apis" ||
                                HasTag(avd.SystemImageTagIds, "google_apis") ||
                                tagDisplay.find("google apis") != std::string::npos;

        avd.Supports16KbPageSize = variant.find("ps16k") != std::string::npos ||
                                   HasTag(avd.SystemImageTagIds, "page_size_16kb") ||
                                   tagDisplayNames.find("16kb") != std::string::npos ||
                                   tagDisplayNames.find("16 kb") != std::string::npos;
    }

    static AvdInfo ExtractAvdInfo(const std::string &avdName) {
        AvdInfo avd;

        const std::string avdRoot = Paths::GetAvdDirectory();
        if (avdRoot.empty()) return avd;

        const std::string path = Paths::JoinPaths({avdRoot, avdName + ".avd"});

        avd.Name = avdName;
        avd.DisplayName = avdName;
        avd.Path = path;

        std::string configPath = Paths::JoinPaths({avd.Path, "config.ini"});

        if (!std::filesystem::exists(configPath)) return avd;

        auto config = ParseConfigFile(configPath);

        if (auto it = config.find("hw.device.name"); it != config.end()) {
            avd.Device = it->second;
        }

        if (auto it = config.find("avd.ini.displayname"); it != config.end() && !it->second.empty()) {
            avd.DisplayName = it->second;
        }

        if (auto it = config.find("abi.type"); it != config.end()) {
            avd.Abi = it->second;
        }

        ExtractSystemImageInfo(avd, config);

        if (auto it = config.find("sdcard.size"); it != config.end()) {
            avd.SdCard = it->second;
        }

        if (auto it = config.find("hw.ramSize"); it != config.end()) {
            avd.RamSize = it->second;
        }

        if (auto it = config.find("hw.cpu.arch"); it != config.end()) {
            avd.Arch = it->second;
        }

        std::string width, height;
        if (auto it = config.find("hw.lcd.width"); it != config.end()) {
            width = it->second;
        }

        if (auto it = config.find("hw.lcd.height"); it != config.end()) {
            height = it->second;
        }

        if (!width.empty() && !height.empty()) {
            std::stringstream ss;
            ss << width << "x" << height;
            avd.ScreenResolution = ss.str();
        }

        if (auto it = config.find("hw.gpu.mode"); it != config.end()) {
            avd.GpuMode = it->second;
        }

        if (auto it = config.find("skin.name"); it != config.end()) {
            avd.SkinName = it->second;
        }

        return avd;
    }

    std::vector<AvdInfo> LoadAvds(const std::vector<std::string> &avdNames) {
        std::vector<AvdInfo> avds;
        avds.reserve(avdNames.size());

        for (const auto &avdName: avdNames) {
            const auto &avd = ExtractAvdInfo(avdName);
            avds.push_back(avd);
        }
        return avds;
    }

    std::vector<std::string> ListAvdNames(const SdkInfo &sdk) {
        std::vector<std::string> avds;
        if (!sdk.IsFound) return avds;

        const std::string output = RunCommandArgs(sdk.EmulatorPath, {"-list-avds"});
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) avds.emplace_back(line);
        }

        return avds;
    }

    bool CreateAvd(const SdkInfo &sdk, const AvdCreationData &data) {
        if (sdk.AvdManagerPath.empty()) return false;
        if (data.Name.empty() || data.SystemImagePackagePath.empty()) return false;

        std::vector<std::string> args = {
            "create",
            "avd",
            "-n",
            data.Name,
            "-k",
            data.SystemImagePackagePath
        };
        if (!data.DeviceId.empty()) {
            args.emplace_back("-d");
            args.push_back(data.DeviceId);
        }
        RunCommandArgs(sdk.AvdManagerPath, args, "no\n");

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string configPath = Paths::JoinPaths({avdDir, data.Name + ".avd", "config.ini"});

        if (std::filesystem::exists(configPath)) {
            std::ofstream file(configPath, std::ios::app);
            if (file.is_open()) {
                if (!data.DisplayName.empty()) {
                    file << "avd.ini.displayname=" << data.DisplayName << "\n";
                }
                if (!data.RamSize.empty()) {
                    file << "hw.ramSize=" << data.RamSize << "\n";
                }
                if (!data.SdCardSize.empty()) {
                    file << "sdcard.size=" << data.SdCardSize << "\n";
                }
                if (!data.GpuMode.empty()) {
                    file << "hw.gpu.mode=" << data.GpuMode << "\n";
                    file << "hw.gpu.enabled=yes\n";
                }
                if (!data.SkinName.empty()) {
                    file << "skin.name=" << data.SkinName << "\n";
                    if (!data.SkinPath.empty()) {
                        file << "skin.path=" << data.SkinPath << "\n";
                    }
                }
            }
        }

        const std::string avdFolder = Paths::JoinPaths({avdDir, data.Name + ".avd"});
        return std::filesystem::exists(avdFolder);
    }

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName) {
        if (sdk.AvdManagerPath.empty()) return false;

        RunCommandArgs(sdk.AvdManagerPath, {"delete", "avd", "-n", avdName});

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
        return !std::filesystem::exists(avdFolder);
    }
}

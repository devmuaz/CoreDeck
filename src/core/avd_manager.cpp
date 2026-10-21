//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "avd_manager.h"
#include "paths.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        bool IsAvdManagerDiagnostic(const std::string &line) {
            return line.starts_with("Error:") || line.starts_with("Warning:");
        }

        std::unordered_map<std::string, std::string> ParseConfigFile(const std::string &path) {
            std::unordered_map<std::string, std::string> config;
            std::ifstream file(path);
            if (!file.is_open()) {
                return config;
            }

            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.empty() || line.at(0) == '#') {
                    continue;
                }

                auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }

                auto key = line.substr(0, eq);
                auto value = line.substr(eq + 1);

                while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                    key.pop_back();
                }
                while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
                    key.erase(key.begin());
                }
                while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                    value.pop_back();
                }
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                    value.erase(value.begin());
                }

                config[key] = value;
            }

            return config;
        }

        std::vector<std::string> SplitConfigList(const std::string &value) {
            std::vector<std::string> items;
            std::stringstream stream(value);
            std::string item;
            while (std::getline(stream, item, ',')) {
                while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
                    item.pop_back();
                }
                while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) {
                    item.erase(item.begin());
                }
                if (!item.empty()) {
                    items.push_back(item);
                }
            }
            return items;
        }

        bool HasTag(const std::vector<std::string> &tags, const std::string &needle) {
            return std::ranges::any_of(tags, [&](const std::string &tag) {
                return LowerCopy(tag) == needle;
            });
        }

        void ExtractSystemImageInfo(AvdInfo &avd, const std::unordered_map<std::string, std::string> &config) {
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

            avd.IsGooglePlayImage =
                variant.find("google_apis_playstore") != std::string::npos ||
                tagId == "google_apis_playstore" ||
                HasTag(avd.SystemImageTagIds, "google_apis_playstore") ||
                tagDisplay.find("play") != std::string::npos;

            avd.IsGoogleApisImage =
                avd.IsGooglePlayImage ||
                variant.find("google_apis") != std::string::npos ||
                tagId == "google_apis" ||
                HasTag(avd.SystemImageTagIds, "google_apis") ||
                tagDisplay.find("google apis") != std::string::npos;

            avd.Supports16KbPageSize =
                variant.find("ps16k") != std::string::npos ||
                HasTag(avd.SystemImageTagIds, "page_size_16kb") ||
                tagDisplayNames.find("16kb") != std::string::npos ||
                tagDisplayNames.find("16 kb") != std::string::npos;
        }

        AvdInfo ExtractAvdInfo(const std::string &avdName) {
            AvdInfo avd;

            const std::string avdRoot = Paths::GetAvdDirectory();
            if (avdRoot.empty()) {
                return avd;
            }

            const std::string path = Paths::JoinPaths({avdRoot, avdName + ".avd"});

            avd.Name = avdName;
            avd.DisplayName = avdName;
            avd.Path = path;

            std::string configPath = Paths::JoinPaths({avd.Path, "config.ini"});

            if (!std::filesystem::exists(configPath)) {
                return avd;
            }

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

            std::string width;
            std::string height;
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

        std::string ConfigKey(const std::string &line) {
            if (line.empty() || line.front() == '#') {
                return {};
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                return {};
            }

            std::string key = line.substr(0, eq);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
                key.pop_back();
            }
            while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
                key.erase(key.begin());
            }
            return key;
        }

        std::string ConfigAssignment(const std::string &key, const std::string &value) {
            std::string assignment = key;
            assignment += '=';
            assignment += value;
            return assignment;
        }

        bool SetConfigValue(const std::string &path, const std::string &key, const std::string &value) {
            std::ifstream input(path);
            if (!input.is_open()) {
                return false;
            }

            std::vector<std::string> lines;
            std::string line;
            bool replaced = false;
            while (std::getline(input, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (ConfigKey(line) == key) {
                    if (!replaced) {
                        lines.push_back(ConfigAssignment(key, value));
                        replaced = true;
                    }
                    continue;
                }
                lines.push_back(line);
            }
            input.close();

            if (!replaced) {
                lines.push_back(ConfigAssignment(key, value));
            }

            std::ofstream output(path, std::ios::trunc);
            if (!output.is_open()) {
                return false;
            }
            for (const std::string &written: lines) {
                output << written << '\n';
            }
            return static_cast<bool>(output);
        }
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
                device.Name.at(0) = static_cast<char>(std::toupper(device.Name.at(0)));
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

    std::vector<AvdInfo> LoadAvds(const std::vector<std::string> &avdNames) {
        std::vector<AvdInfo> avds;
        avds.reserve(avdNames.size());

        for (const auto &avdName: avdNames) {
            const auto &avd = ExtractAvdInfo(avdName);
            avds.push_back(avd);
        }
        return avds;
    }

    std::vector<std::string> ParseAvdManagerAvdList(const std::string &output) {
        std::vector<std::string> avds;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty() || IsAvdManagerDiagnostic(line)) {
                continue;
            }
            avds.push_back(line);
        }
        return avds;
    }

    std::vector<std::string> ListAvdNames(const SdkInfo &sdk) {
        if (sdk.AvdManagerPath.empty()) {
            return {};
        }

        const std::string output = RunCommandArgs(sdk.AvdManagerPath, {"list", "avd", "-c"}, "", sdk.ToolEnv);
        return ParseAvdManagerAvdList(output);
    }

    bool CreateAvd(const SdkInfo &sdk, const AvdCreationData &data) {
        if (sdk.AvdManagerPath.empty()) {
            return false;
        }
        if (data.Name.empty() || data.SystemImagePackagePath.empty()) {
            return false;
        }

        std::vector<std::string> args = {
            "create",
            "avd",
            "-n",
            data.Name,
            "-k",
            data.SystemImagePackagePath,
        };
        if (!data.DeviceId.empty()) {
            args.emplace_back("-d");
            args.push_back(data.DeviceId);
        }
        RunCommandArgs(sdk.AvdManagerPath, args, "no\n", sdk.ToolEnv);

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string configPath = Paths::JoinPaths({avdDir, data.Name + ".avd", "config.ini"});

        if (std::filesystem::exists(configPath)) {
            SetConfigValue(configPath, "hw.keyboard", "yes");
            if (!data.DisplayName.empty()) {
                SetConfigValue(configPath, "avd.ini.displayname", data.DisplayName);
            }
            if (!data.RamSize.empty()) {
                SetConfigValue(configPath, "hw.ramSize", data.RamSize);
            }
            if (!data.SdCardSize.empty()) {
                SetConfigValue(configPath, "sdcard.size", data.SdCardSize);
            }
            if (!data.GpuMode.empty()) {
                SetConfigValue(configPath, "hw.gpu.mode", data.GpuMode);
                SetConfigValue(configPath, "hw.gpu.enabled", "yes");
            }
            if (!data.SkinName.empty()) {
                SetConfigValue(configPath, "skin.name", data.SkinName);
                if (!data.SkinPath.empty()) {
                    SetConfigValue(configPath, "skin.path", data.SkinPath);
                }
            }
        }

        const std::string avdFolder = Paths::JoinPaths({avdDir, data.Name + ".avd"});
        return std::filesystem::exists(avdFolder);
    }

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName) {
        if (sdk.AvdManagerPath.empty()) {
            return false;
        }

        RunCommandArgs(sdk.AvdManagerPath, {"delete", "avd", "-n", avdName}, "", sdk.ToolEnv);

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
        return !std::filesystem::exists(avdFolder);
    }
}

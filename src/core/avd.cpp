//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <sstream>

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

    static AvdInfo ExtractAvdInfo(const std::string &avdName) {
        AvdInfo avd;

        const std::string avdRoot = Paths::GetAvdDirectory();
        if (avdRoot.empty()) return avd;

        const std::string path = Paths::JoinPaths({avdRoot, avdName + ".avd"});

        avd.Name = avdName;
        avd.Path = path;

        std::string configPath = Paths::JoinPaths({avd.Path, "config.ini"});

        if (!std::filesystem::exists(configPath)) return avd;

        auto config = ParseConfigFile(configPath);

        if (auto it = config.find("hw.device.name"); it != config.end()) {
            avd.Device = it->second;
        }

        if (auto it = config.find("avd.ini.displayname"); it != config.end()) {
            avd.DisplayName = it->second;
        }

        if (auto it = config.find("image.sysdir.1"); it != config.end()) {
            auto &sysdir = it->second;
            if (auto start = sysdir.find("android-"); start != std::string::npos) {
                start += 8;
                if (const auto end = sysdir.find('/', start); end != std::string::npos) {
                    avd.ApiLevel = sysdir.substr(start, end - start);
                }
            }
        }

        if (auto it = config.find("abi.type"); it != config.end()) {
            avd.Abi = it->second;
        }

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

        const std::string cmd = StrConcat("\"", sdk.EmulatorPath, "\" -list-avds");
        const std::string output = RunCommand(cmd);
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) avds.emplace_back(line);
        }

        return avds;
    }

    std::vector<SystemImage> ListSystemImages(const SdkInfo &sdk) {
        std::vector<SystemImage> images;
        if (sdk.SdkPath.empty()) return images;

        const std::string sysImgRoot = Paths::JoinPaths({sdk.SdkPath, "system-images"});
        if (!std::filesystem::exists(sysImgRoot)) return images;

        // Structure: system-images/android-XX/variant/abi/
        for (const auto &apiEntry: std::filesystem::directory_iterator(sysImgRoot)) {
            if (!apiEntry.is_directory()) continue;
            const std::string apiDirName = apiEntry.path().filename().string();

            std::string apiLevel;
            if (apiDirName.starts_with("android-")) {
                apiLevel = apiDirName.substr(8);
            } else {
                continue;
            }

            for (const auto &variantEntry: std::filesystem::directory_iterator(apiEntry.path())) {
                if (!variantEntry.is_directory()) continue;
                const std::string variant = variantEntry.path().filename().string();

                for (const auto &abiEntry: std::filesystem::directory_iterator(variantEntry.path())) {
                    if (!abiEntry.is_directory()) continue;
                    const std::string abi = abiEntry.path().filename().string();

                    // Verify it has a system.img or similar marker
                    const std::string sysImg = Paths::JoinPaths({abiEntry.path().string(), "system.img"});
                    if (!std::filesystem::exists(sysImg)) continue;

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

    std::vector<DeviceProfile> ListDeviceProfiles(const SdkInfo &sdk) {
        std::vector<DeviceProfile> devices;

        if (sdk.AvdManagerPath.empty()) return devices;

        const std::string cmd = StrConcat("\"", sdk.AvdManagerPath, "\" list device -c");
        const std::string output = RunCommand(cmd);
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty()) continue;

            DeviceProfile device;
            device.Id = line;
            device.Name = line;
            std::ranges::replace(device.Name, '_', ' ');
            if (!device.Name.empty()) {
                device.Name[0] = static_cast<char>(std::toupper(device.Name[0]));
            }
            devices.push_back(device);
        }

        return devices;
    }

    bool CreateAvd(const SdkInfo &sdk, const CreateAvdParams &params) {
        if (sdk.AvdManagerPath.empty()) return false;
        if (params.Name.empty() || params.SystemImagePackagePath.empty()) return false;

        std::string cmd = StrConcat(
            "echo no | \"", sdk.AvdManagerPath, "\" create avd -n \"", params.Name, "\" -k \"",
            params.SystemImagePackagePath, "\""
        );

        if (!params.DeviceId.empty()) cmd = StrConcat(cmd, " -d \"", params.DeviceId, "\"");
        RunCommand(cmd);

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string configPath = Paths::JoinPaths({avdDir, params.Name + ".avd", "config.ini"});

        if (std::filesystem::exists(configPath)) {
            std::ofstream file(configPath, std::ios::app);
            if (file.is_open()) {
                if (!params.DisplayName.empty()) {
                    file << "avd.ini.displayname=" << params.DisplayName << "\n";
                }
                if (!params.RamSize.empty()) {
                    file << "hw.ramSize=" << params.RamSize << "\n";
                }
                if (!params.SdCardSize.empty()) {
                    file << "sdcard.size=" << params.SdCardSize << "\n";
                }
                if (!params.GpuMode.empty()) {
                    file << "hw.gpu.mode=" << params.GpuMode << "\n";
                    file << "hw.gpu.enabled=yes\n";
                }
            }
        }

        const std::string avdFolder = Paths::JoinPaths({avdDir, params.Name + ".avd"});
        return std::filesystem::exists(avdFolder);
    }

    bool DeleteAvd(const SdkInfo &sdk, const std::string &avdName) {
        if (!sdk.AvdManagerPath.empty()) {
            const std::string cmd = StrConcat("\"", sdk.AvdManagerPath, "\" delete avd -n ", avdName);
            RunCommand(cmd);
        } else {
            // Fallback: manually delete the AVD files
            const std::string avdDir = Paths::GetAvdDirectory();
            if (avdDir.empty()) return false;

            const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
            const std::string avdIni = Paths::JoinPaths({avdDir, avdName + ".ini"});

            std::error_code ec;
            std::filesystem::remove_all(avdFolder, ec);
            if (ec) return false;

            std::filesystem::remove(avdIni, ec);
        }

        const std::string avdDir = Paths::GetAvdDirectory();
        const std::string avdFolder = Paths::JoinPaths({avdDir, avdName + ".avd"});
        return !std::filesystem::exists(avdFolder);
    }
}

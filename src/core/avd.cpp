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

    bool CreateAvd(const SdkInfo &sdk, const AvdCreationData &data) {
        if (sdk.AvdManagerPath.empty()) return false;
        if (data.Name.empty() || data.SystemImagePackagePath.empty()) return false;

        std::string cmd = StrConcat(
            "echo no | \"", sdk.AvdManagerPath, "\" create avd -n \"", data.Name, "\" -k \"",
            data.SystemImagePackagePath, "\""
        );

        if (!data.DeviceId.empty()) cmd = StrConcat(cmd, " -d \"", data.DeviceId, "\"");
        RunCommand(cmd);

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
            }
        }

        const std::string avdFolder = Paths::JoinPaths({avdDir, data.Name + ".avd"});
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

//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include "avd.h"
#include "paths.h"

#include <fstream>
#include <unordered_map>

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

    std::vector<AvdInfo> LoadAvds(const std::vector<std::string> &avdNames) {
        std::vector<AvdInfo> avds;

        const std::string avdRoot = Paths::GetAvdDirectory();
        if (avdRoot.empty()) return avds;
        for (const auto &avdName: avdNames) {
            AvdInfo avd;

            avd.Name = avdName;
            avd.Path = Paths::JoinPaths({avdRoot, avdName + ".avd"});

            std::string configPath = Paths::JoinPaths({avd.Path, "config.ini"});

            if (!std::filesystem::exists(configPath)) {
                avds.push_back(avd);
                continue;
            }

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
                avd.ScreenResolution = std::format("{}x{}", width, height);
            }

            if (auto it = config.find("hw.gpu.mode"); it != config.end()) {
                avd.GpuMode = it->second;
            }

            avds.push_back(std::move(avd));
        }
        return avds;
    }
}

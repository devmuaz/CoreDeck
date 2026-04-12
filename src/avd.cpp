//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include "avd.h"
#include "paths.h"

#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <sstream>

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
        avds.reserve(avdNames.size());

        for (const auto &avdName: avdNames) {
            const auto &avd = LoadAvd(avdName);
            avds.push_back(avd);
        }
        return avds;
    }

    static AvdInfo LoadAvd(const std::string &avdName) {
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
}

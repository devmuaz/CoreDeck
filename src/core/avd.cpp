//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
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

    std::vector<RemoteSystemImage> ListRemoteSystemImages(
        const SdkInfo &sdk,
        const std::vector<SystemImage> &installedImages
    ) {
        std::vector<RemoteSystemImage> results;
        if (sdk.SdkManagerPath.empty()) return results;

        const std::string cmd = StrConcat("\"", sdk.SdkManagerPath, "\" --list 2>&1");
        const std::string output = RunCommand(cmd);

        std::unordered_map<std::string, bool> installedSet;
        for (const auto &img: installedImages) {
            installedSet[img.PackagePath] = true;
        }

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            if (!line.starts_with("system-images;")) continue;

            std::string packagePath;
            if (auto pipe = line.find('|'); pipe != std::string::npos) {
                packagePath = line.substr(0, pipe);
            } else {
                packagePath = line;
            }

            while (!packagePath.empty() && (packagePath.back() == ' ' || packagePath.back() == '\t')) {
                packagePath.pop_back();
            }

            std::vector<std::string> parts;
            std::istringstream partStream(packagePath);
            std::string part;
            while (std::getline(partStream, part, ';')) {
                parts.push_back(part);
            }
            if (parts.size() < 4) continue;

            RemoteSystemImage img;
            img.PackagePath = packagePath;

            if (parts[1].starts_with("android-")) {
                img.ApiLevel = parts[1].substr(8);
            } else {
                continue;
            }

            img.Variant = parts[2];
            img.Abi = parts[3];
            img.IsInstalled = installedSet.contains(packagePath);
            img.DisplayName = StrConcat("Android ", img.ApiLevel, " (", img.Variant, ", ", img.Abi, ")");

            results.push_back(std::move(img));
        }

        std::ranges::sort(results, [](const RemoteSystemImage &a, const RemoteSystemImage &b) {
            const int apiA = static_cast<int>(std::strtol(a.ApiLevel.c_str(), nullptr, 10));
            const int apiB = static_cast<int>(std::strtol(b.ApiLevel.c_str(), nullptr, 10));
            if (apiA != apiB) return apiA > apiB;
            if (a.IsInstalled != b.IsInstalled) return a.IsInstalled;
            return a.DisplayName < b.DisplayName;
        });

        return results;
    }

    static void ParseProgressLine(const std::string &line, const std::shared_ptr<InstallProgressData> &progress) {
        if (!progress) return;

        const auto bracket = line.find('[');
        const auto closeBracket = line.find(']', bracket);
        if (bracket == std::string::npos || closeBracket == std::string::npos) {
            if (!line.empty()) {
                std::lock_guard lock(progress->Mutex);
                progress->DetailText = line;
            }
            return;
        }

        auto afterBracket = line.substr(closeBracket + 1);
        const auto start = afterBracket.find_first_not_of(" \t");
        if (start == std::string::npos) return;
        afterBracket = afterBracket.substr(start);

        const auto pctEnd = afterBracket.find('%');
        if (pctEnd == std::string::npos) return;

        const int pct = static_cast<int>(std::strtol(afterBracket.substr(0, pctEnd).c_str(), nullptr, 10));

        std::string description;
        if (pctEnd + 1 < afterBracket.size()) {
            description = afterBracket.substr(pctEnd + 1);
            if (const auto ds = description.find_first_not_of(" \t"); ds != std::string::npos) {
                description = description.substr(ds);
            }
        }

        std::lock_guard lock(progress->Mutex);
        progress->Percent = static_cast<float>(pct) / 100.0f;
        if (!description.empty()) progress->StatusText = description;
        progress->DetailText = line;
    }

    bool InstallSystemImage(
        const SdkInfo &sdk,
        const std::string &packagePath,
        const std::shared_ptr<InstallProgressData> &progress
    ) {
        if (sdk.SdkManagerPath.empty() || packagePath.empty()) return false;

        const std::string acceptCmd = StrConcat(
#ifdef _WIN32
            "echo y| \"", sdk.SdkManagerPath, "\" --licenses 2>&1"
#else
            "yes | \"", sdk.SdkManagerPath, "\" --licenses 2>&1"
#endif
        );
        RunCommand(acceptCmd);

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->StatusText = "Starting download...";
            progress->Percent = 0.0f;
        }

        const std::string cmd = StrConcat("\"", sdk.SdkManagerPath, "\" --install \"", packagePath, "\" 2>&1");
#ifdef _WIN32
        FILE *pipe = _popen(cmd.c_str(), "r");
#else
        FILE *pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) return false;

        std::array<char, 512> buf{};
        std::string partial;

        while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
            partial += buf.data();

            std::size_t pos;
            while ((pos = partial.find_first_of("\n\r")) != std::string::npos) {
                if (auto line = partial.substr(0, pos); !line.empty()) {
                    ParseProgressLine(line, progress);
                }
                if (auto next = partial.find_first_not_of("\n\r", pos); next == std::string::npos) {
                    partial.clear();
                } else {
                    partial = partial.substr(next);
                }
            }
        }

        if (!partial.empty()) {
            ParseProgressLine(partial, progress);
        }

#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif

        // Verify
        std::string fsPath = packagePath;
        std::ranges::replace(fsPath, ';', '/');
        const std::string sysImg = Paths::JoinPaths({sdk.SdkPath, fsPath, "system.img"});
        const bool ok = std::filesystem::exists(sysImg);

        if (progress) {
            std::lock_guard lock(progress->Mutex);
            progress->Finished = true;
            progress->Succeeded = ok;
            progress->Percent = ok ? 1.0f : progress->Percent;
            progress->StatusText = ok ? "Installation complete" : "Installation failed";
        }

        return ok;
    }

    bool UninstallSystemImage(const SdkInfo &sdk, const std::string &packagePath) {
        if (sdk.SdkManagerPath.empty() || packagePath.empty()) return false;

#ifdef _WIN32
        const std::string cmd = StrConcat("echo y| \"", sdk.SdkManagerPath, "\" --uninstall \"", packagePath, "\" 2>&1");
#else
        const std::string cmd = StrConcat("yes | \"", sdk.SdkManagerPath, "\" --uninstall \"", packagePath, "\" 2>&1");
#endif
        RunCommand(cmd);

        std::string fsPath = packagePath;
        std::ranges::replace(fsPath, ';', '/');
        const std::string sysImg = Paths::JoinPaths({sdk.SdkPath, fsPath, "system.img"});
        return !std::filesystem::exists(sysImg);
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

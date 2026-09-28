//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>
#include <vector>

#include "sdk.h"
#include "paths.h"

namespace CoreDeck {
    namespace {
        std::string FindCmdlineTool(const std::string &binDir, const std::string &name) {
#ifdef _WIN32
            for (const auto *ext: {".bat", ".exe"}) {
                const std::string candidate = Paths::JoinPaths({binDir, name + ext});
                if (std::filesystem::exists(candidate)) return candidate;
            }
            return "";
#else
            const std::string candidate = Paths::JoinPaths({binDir, name});
            return std::filesystem::exists(candidate) ? candidate : "";
#endif
        }

        std::vector<int> BuildToolsVersionNumbers(const std::string &name) {
            std::vector<int> parts;
            int current = 0;
            bool inNumber = false;
            for (const char c: name) {
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    inNumber = true;
                    current = (current * 10) + (c - '0');
                } else if (c == '.' && inNumber) {
                    parts.push_back(current);
                    current = 0;
                    inNumber = false;
                } else {
                    break;
                }
            }
            if (inNumber) {
                parts.push_back(current);
            }
            return parts;
        }

        bool IsNewerBuildTools(const std::string &candidate, const std::string &current) {
            const std::vector<int> left = BuildToolsVersionNumbers(candidate);
            const std::vector<int> right = BuildToolsVersionNumbers(current);
            const std::size_t count = std::max(left.size(), right.size());
            for (std::size_t i = 0; i < count; ++i) {
                const int leftPart = i < left.size() ? left.at(i) : 0;
                const int rightPart = i < right.size() ? right.at(i) : 0;
                if (leftPart != rightPart) {
                    return leftPart > rightPart;
                }
            }
            const bool leftPrerelease = candidate.find('-') != std::string::npos;
            const bool rightPrerelease = current.find('-') != std::string::npos;
            if (leftPrerelease != rightPrerelease) {
                return !leftPrerelease;
            }
            return candidate > current;
        }
    }

    std::string FindInstalledAapt2(const std::string &sdkPath) {
        if (sdkPath.empty()) {
            return {};
        }
        const std::filesystem::path root = std::filesystem::path(sdkPath) / "build-tools";
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) {
            return {};
        }

        const std::string binaryName = "aapt2" + Paths::GetExecutableExtension();
        std::string bestPath;
        std::string bestVersion;
        for (const auto &entry: std::filesystem::directory_iterator(root, ec)) {
            if (ec || !entry.is_directory(ec)) {
                continue;
            }
            const std::filesystem::path binary = entry.path() / binaryName;
            if (!std::filesystem::exists(binary, ec)) {
                continue;
            }
            const std::string version = entry.path().filename().string();
            if (bestPath.empty() || IsNewerBuildTools(version, bestVersion)) {
                bestPath = binary.string();
                bestVersion = version;
            }
        }
        return bestPath;
    }

    SdkInfo DetectAndroidSdk() {
        std::string sdkPath;

        const std::string savedPath = Paths::Onboarding::LoadSdkPathOverride();
        if (!savedPath.empty() && std::filesystem::exists(savedPath)) {
            sdkPath = savedPath;
        } else {
            const char *sdkEnv = std::getenv("ANDROID_HOME"); // NOLINT(concurrency-mt-unsafe)
            if (!sdkEnv) {
                sdkEnv = std::getenv("ANDROID_SDK_ROOT"); // NOLINT(concurrency-mt-unsafe)
            }

            if (sdkEnv) {
                sdkPath = sdkEnv;
            } else {
                const std::string defaultPath = Paths::GetAndroidSdkDefaultPath();
                if (std::filesystem::exists(defaultPath)) {
                    sdkPath = defaultPath;
                }
            }
        }

        return ProbeAndroidSdk(sdkPath);
    }

    SdkInfo ProbeAndroidSdk(const std::string &sdkPath) {
        SdkInfo sdk;
        sdk.SdkPath = sdkPath;

        if (sdk.SdkPath.empty()) {
            return sdk;
        }

        sdk.EmulatorPath = Paths::JoinPaths({sdk.SdkPath, "emulator", "emulator" + Paths::GetExecutableExtension()});

        std::vector<std::string> binDirs;
        binDirs.push_back(Paths::JoinPaths({sdk.SdkPath, "cmdline-tools", "latest", "bin"}));
        const std::string cmdlineRoot = Paths::JoinPaths({sdk.SdkPath, "cmdline-tools"});
        if (std::filesystem::exists(cmdlineRoot) && std::filesystem::is_directory(cmdlineRoot)) {
            for (const auto &entry: std::filesystem::directory_iterator(cmdlineRoot)) {
                if (!entry.is_directory() || entry.path().filename() == "latest") {
                    continue;
                }
                binDirs.push_back(Paths::JoinPaths({entry.path().string(), "bin"}));
            }
        }

        const auto takeFirst = [&binDirs](const std::string &name) {
            for (const std::string &binDir: binDirs) {
                const std::string candidate = FindCmdlineTool(binDir, name);
                if (!candidate.empty()) {
                    return candidate;
                }
            }
            return std::string{};
        };
        sdk.AvdManagerPath = takeFirst("avdmanager");
        sdk.SdkManagerPath = takeFirst("sdkmanager");
        sdk.ApkAnalyzerPath = takeFirst("apkanalyzer");
        sdk.Aapt2Path = FindInstalledAapt2(sdk.SdkPath);

        if (std::filesystem::exists(sdk.EmulatorPath)) {
            sdk.IsFound = true;
        }
        return sdk;
    }

    void RefreshAndroidSdk(SdkInfo &sdk) {
        // Copy ToolEnv before probing so a background sdkmanager call still sees
        // the JDK environment while the filesystem walk runs. JDK feature
        // releases such as 27 otherwise fail the Windows command-line tools'
        // own version check.
        const std::string sdkPath = sdk.SdkPath;
        const EnvVars toolEnv = sdk.ToolEnv;
        SdkInfo probed = sdkPath.empty() ? DetectAndroidSdk() : ProbeAndroidSdk(sdkPath);
        probed.ToolEnv = toolEnv;
        sdk = std::move(probed);
    }
}

//
// Created by AbdulMuaz Aqeel on 02/04/2026.
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>
#include <vector>

#include "sdk.h"
#include "platform/paths.h"

namespace CoreDeck::Core {
    namespace {
        std::string FindCmdlineTool(const std::string &binDir, const std::string &name) {
#ifdef _WIN32
            for (const auto *ext: {".bat", ".exe"}) {
                const std::string candidate = Platform::JoinPaths({binDir, name + ext});
                if (std::filesystem::exists(candidate)) return candidate;
            }
            return "";
#else
            const std::string candidate = Platform::JoinPaths({binDir, name});
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

        std::filesystem::path FindBuildToolBinary(
            const std::filesystem::path &versionDir, const std::string &name, std::error_code &ec
        ) {
#ifdef _WIN32
            if (name == "apksigner") {
                for (const auto *ext: {".bat", ".exe"}) {
                    const std::filesystem::path candidate = versionDir / (name + ext);
                    std::error_code probeEc;
                    if (std::filesystem::exists(candidate, probeEc)) return candidate;
                }
                return {};
            }
#endif
            std::filesystem::path candidate = versionDir / Platform::WithExecutableExtension(name);
            if (!std::filesystem::exists(candidate, ec)) return {};
            return candidate;
        }
    }

    std::string FindInstalledBuildTool(const std::string &sdkPath, const std::string &name) {
        if (sdkPath.empty() || name.empty()) {
            return {};
        }
        const std::filesystem::path root = std::filesystem::path(sdkPath) / "build-tools";
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) {
            return {};
        }

        std::string bestPath;
        std::string bestVersion;
        for (const auto &entry: std::filesystem::directory_iterator(root, ec)) {
            if (ec || !entry.is_directory(ec)) {
                continue;
            }
            const std::filesystem::path binary = FindBuildToolBinary(entry.path(), name, ec);
            if (binary.empty()) {
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

    std::string FindInstalledAapt2(const std::string &sdkPath) {
        return FindInstalledBuildTool(sdkPath, "aapt2");
    }

    SdkInfo DetectAndroidSdk(const std::string_view savedPath) {
        std::string sdkPath;

        if (!savedPath.empty() && std::filesystem::exists(std::string(savedPath))) {
            sdkPath = std::string(savedPath);
        } else {
            const char *sdkEnv = std::getenv(ENV_ANDROID_HOME); // NOLINT(concurrency-mt-unsafe)
            if (!sdkEnv) {
                sdkEnv = std::getenv(ENV_ANDROID_SDK_ROOT); // NOLINT(concurrency-mt-unsafe)
            }

            if (sdkEnv) {
                sdkPath = sdkEnv;
            } else {
                const std::string defaultPath = Core::GetAndroidSdkDefaultPath();
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

        sdk.EmulatorPath = Platform::JoinPaths({sdk.SdkPath, "emulator", Platform::WithExecutableExtension("emulator")});

        std::vector<std::string> binDirs;
        binDirs.push_back(Platform::JoinPaths({sdk.SdkPath, "cmdline-tools", "latest", "bin"}));
        const std::string cmdlineRoot = Platform::JoinPaths({sdk.SdkPath, "cmdline-tools"});
        if (std::filesystem::exists(cmdlineRoot) && std::filesystem::is_directory(cmdlineRoot)) {
            for (const auto &entry: std::filesystem::directory_iterator(cmdlineRoot)) {
                if (!entry.is_directory() || entry.path().filename() == "latest") {
                    continue;
                }
                binDirs.push_back(Platform::JoinPaths({entry.path().string(), "bin"}));
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
        sdk.Aapt2Path = FindInstalledBuildTool(sdk.SdkPath, "aapt2");
        sdk.ZipalignPath = FindInstalledBuildTool(sdk.SdkPath, "zipalign");
        sdk.ApksignerPath = FindInstalledBuildTool(sdk.SdkPath, "apksigner");

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
        const Platform::EnvVars toolEnv = sdk.ToolEnv;
        if (sdkPath.empty()) {
            return;
        }
        SdkInfo probed = ProbeAndroidSdk(sdkPath);
        probed.ToolEnv = toolEnv;
        sdk = std::move(probed);
    }
    std::string GetAndroidSdkDefaultPath() {
        const auto platform = Platform::GetCurrentOs();
        const std::string home = Platform::GetHomeDirectory();

        if (home.empty()) {
            return "";
        }

        switch (platform) {
            case Platform::Os::Windows: {
                const char *localAppData = std::getenv(Platform::ENV_LOCALAPPDATA); // NOLINT(concurrency-mt-unsafe)
                if (localAppData) {
                    return Platform::JoinPaths({std::string(localAppData), "Android", "Sdk"});
                }
                return Platform::JoinPaths({home, "AppData", "Local", "Android", "Sdk"});
            }
            case Platform::Os::MacOS:
                return Platform::JoinPaths({home, "Library", "Android", "sdk"});
            case Platform::Os::Linux:
            default:
                return Platform::JoinPaths({home, "Android", "Sdk"});
        }
    }
    bool ValidateSdkPath(const std::string &path) {
        if (path.empty()) {
            return false;
        }
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
            return false;
        }

        const std::string emulatorBinary = Platform::JoinPaths({path, "emulator", Platform::WithExecutableExtension("emulator")});
        return std::filesystem::exists(emulatorBinary);
    }

}

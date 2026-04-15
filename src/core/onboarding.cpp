//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "onboarding.h"

#include <filesystem>
#include <fstream>

#include "paths.h"

namespace CoreDeck {
    static constexpr const char *FIRST_RUN_FLAG_FILE = "first_run_complete";
    static constexpr const char *SDK_OVERRIDE_PATH = "sdk_path";

    bool IsFirstRunComplete() {
        const std::string path = Paths::GetAppConfigPath(FIRST_RUN_FLAG_FILE);
        return std::filesystem::exists(path);
    }

    void MarkFirstRunComplete() {
        const std::string path = Paths::GetAppConfigPath(FIRST_RUN_FLAG_FILE);
        std::ofstream out(path);
    }

    bool ValidateSdkPath(const std::string &path) {
        if (path.empty()) return false;
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) return false;

        const std::string emulatorBinary = Paths::JoinPaths({
            path, "emulator", "emulator" + Paths::GetExecutableExtension()
        });
        return std::filesystem::exists(emulatorBinary);
    }

    std::string LoadSdkPathOverride() {
        const std::string path = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
        if (!std::filesystem::exists(path)) return {};

        std::ifstream in(path);
        std::string value;
        std::getline(in, value);
        return value;
    }

    void SaveSdkPathOverride(const std::string &path) {
        const std::string file = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
        std::ofstream out(file);
        out << path;
    }

    void ClearSdkPathOverride() {
        const std::string file = Paths::GetAppConfigPath(SDK_OVERRIDE_PATH);
        if (std::filesystem::exists(file)) {
            std::filesystem::remove(file);
        }
    }
}

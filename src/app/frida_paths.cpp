//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "app/frida_paths.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/app_paths.h"
#include "platform/paths.h"

namespace CoreDeck::App {
    namespace {
        std::string ReadText(const std::filesystem::path &path) {
            const std::ifstream input(path);
            if (!input) {
                return {};
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            std::string text = buffer.str();
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }
            return text;
        }
    }

    std::string FridaServerCacheDirectory(const std::string &version) {
        if (version.empty()) {
            return {};
        }
        return GetAppConfigPath(Platform::JoinPaths({"frida", "servers", version}));
    }

    std::string FridaScriptsDirectory() {
        return GetAppConfigPath(Platform::JoinPaths({"frida", "scripts"}));
    }

    std::string EnsureFridaProject(std::string &error) {
        const std::string assets = Platform::JoinPaths({GetAssetsDirectory(), "frida", "project"});
        std::string dest = GetAppConfigPath(Platform::JoinPaths({"frida", "project"}));
        if (assets.empty() || dest.empty()) {
            error = "The Frida project directory is unavailable.";
            return {};
        }
        const std::string assetStamp = ReadText(std::filesystem::path(assets) / "STAMP");
        if (assetStamp.empty()) {
            error = "CoreDeck was built without the Frida agent project.";
            return {};
        }
        std::error_code failure;
        const std::string destStamp = ReadText(std::filesystem::path(dest) / "STAMP");
        if (assetStamp != destStamp) {
            std::filesystem::remove_all(dest, failure);
            std::filesystem::create_directories(std::filesystem::path(dest).parent_path(), failure);
            std::filesystem::copy(
                assets,
                dest,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                failure
            );
            if (failure) {
                error = failure.message();
                return {};
            }
        }
        std::filesystem::create_directories(FridaScriptsDirectory(), failure);
        return dest;
    }

    std::string ResetFridaProject(std::string &error) {
        const std::string dest = GetAppConfigPath(Platform::JoinPaths({"frida", "project"}));
        if (dest.empty()) {
            error = "The Frida project directory is unavailable.";
            return {};
        }
        std::error_code failure;
        std::filesystem::remove_all(dest, failure);
        if (failure) {
            error = failure.message();
            return {};
        }
        return EnsureFridaProject(error);
    }
}

//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "app/frida_paths.h"

#include <filesystem>

#include "app/app_paths.h"
#include "platform/paths.h"

namespace CoreDeck::App {
    namespace {
        bool SameDirectory(const std::string &left, const std::string &right) {
            std::error_code error;
            const std::filesystem::path canonicalLeft = std::filesystem::weakly_canonical(left, error);
            const std::filesystem::path canonicalRight = std::filesystem::weakly_canonical(right, error);
            if (!canonicalLeft.empty() && !canonicalRight.empty()) {
                return canonicalLeft == canonicalRight;
            }
            return std::filesystem::path(left).lexically_normal() == std::filesystem::path(right).lexically_normal();
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

    std::string BundledFridaProject(std::string &error) {
        std::string assets = Platform::JoinPaths({GetAssetsDirectory(), "frida", "project"});
        if (assets.empty()) {
            error = "The Frida project directory is unavailable.";
            return {};
        }
        const std::filesystem::path root(assets);
        std::error_code missing;
        const bool agent = std::filesystem::is_regular_file(root / "agent" / "coredeck_agent.js", missing);
        const bool bridge = std::filesystem::is_regular_file(root / "node_modules" / "frida-java-bridge" / "package.json", missing);
        if (!agent || !bridge) {
            error = "CoreDeck was built without the Frida agent project.";
            return {};
        }
        std::error_code failure;
        std::filesystem::create_directories(FridaScriptsDirectory(), failure);
        return assets;
    }

    std::string ResetFridaProject(const std::string &target, std::string &error) {
        std::string bundled = BundledFridaProject(error);
        if (bundled.empty()) {
            return {};
        }
        if (target.empty() || SameDirectory(target, bundled)) {
            return bundled;
        }
        std::error_code failure;
        std::filesystem::remove_all(target, failure);
        if (failure) {
            error = failure.message();
            return {};
        }
        std::filesystem::create_directories(
            std::filesystem::path(target).parent_path(),
            failure
        );
        std::filesystem::copy(
            bundled,
            target,
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
            failure
        );
        if (failure) {
            error = failure.message();
            return {};
        }
        return target;
    }
}

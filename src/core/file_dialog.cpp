//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <filesystem>

#include "tinyfiledialogs.h"

#include "file_dialog.h"

namespace CoreDeck::FileDialog {
    namespace {
        // macOS `choose folder` aborts the whole dialog when default location
        // does not exist. Walk up to the nearest real directory so Browse still opens.
        std::string ExistingDirectoryHint(const std::string &defaultPath) {
            if (defaultPath.empty()) {
                return {};
            }

            std::filesystem::path path(defaultPath);
            std::error_code error;
            while (!path.empty()) {
                if (std::filesystem::is_directory(path, error)) {
                    return path.string();
                }
                error.clear();
                const std::filesystem::path parent = path.parent_path();
                if (parent == path) {
                    break;
                }
                path = parent;
            }
            return {};
        }
    }

    std::optional<std::string> PickDirectory(const std::string &title, const std::string &defaultPath) {
        const std::string hint = ExistingDirectoryHint(defaultPath);
        const char *result = tinyfd_selectFolderDialog(
            title.c_str(),
            hint.empty() ? nullptr : hint.c_str()
        );

        if (result == nullptr) {
            return std::nullopt;
        }
        return std::string(result);
    }

    std::optional<std::string> SaveFile(
        const std::string &title,
        const std::string &defaultPath,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns
    ) {
        std::vector<const char *> patternPtrs;
        patternPtrs.reserve(patterns.size());
        for (const auto &pattern: patterns) {
            patternPtrs.push_back(pattern.c_str());
        }

        const char *result = tinyfd_saveFileDialog(
            title.c_str(),
            defaultPath.empty() ? "" : defaultPath.c_str(),
            static_cast<int>(patternPtrs.size()),
            patternPtrs.empty() ? nullptr : patternPtrs.data(),
            filterDescription.empty() ? nullptr : filterDescription.c_str()
        );

        if (result == nullptr) {
            return std::nullopt;
        }
        return std::string(result);
    }

    std::optional<std::string> PickFile(
        const std::string &title,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns,
        const std::string &defaultPath
    ) {
        std::vector<const char *> patternPtrs;
        patternPtrs.reserve(patterns.size());
        for (const auto &pattern: patterns) {
            patternPtrs.push_back(pattern.c_str());
        }

        const char *result = tinyfd_openFileDialog(
            title.c_str(),
            defaultPath.empty() ? "" : defaultPath.c_str(),
            static_cast<int>(patternPtrs.size()),
            patternPtrs.empty() ? nullptr : patternPtrs.data(),
            filterDescription.empty() ? nullptr : filterDescription.c_str(),
            0
        );

        if (result == nullptr) {
            return std::nullopt;
        }
        return std::string(result);
    }
}

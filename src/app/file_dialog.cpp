//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "app/file_dialog.h"

#include <filesystem>
#include <string>
#include <vector>

#include <nfd.h>

namespace CoreDeck::App::FileDialog {
    namespace {
        // Open the dialog in the nearest directory that exists when a saved path is missing.
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

        struct DialogStart {
            std::string Directory;
            std::string Name;
        };

        DialogStart SplitStart(const std::string &defaultPath) {
            DialogStart start;
            if (defaultPath.empty()) {
                return start;
            }

            const std::filesystem::path path(defaultPath);
            std::error_code error;
            if (std::filesystem::is_directory(path, error)) {
                start.Directory = path.string();
                return start;
            }

            start.Name = path.filename().string();
            start.Directory = ExistingDirectoryHint(path.parent_path().string());
            return start;
        }

        // NFD_Quit restores the macOS activation policy and deactivates a
        // running app, so keep it initialized.
        bool DialogsReady() {
            static const bool READY = NFD_Init() == NFD_OKAY;
            return READY;
        }

        std::optional<std::string> TakePath(const nfdresult_t result, nfdu8char_t *path) {
            if (result != NFD_OKAY || path == nullptr) {
                return std::nullopt;
            }
            std::string value(path);
            NFD_FreePathU8(path);
            return value;
        }

        std::string FilterSpec(const std::vector<std::string> &patterns) {
            std::string spec;
            for (const std::string &pattern: patterns) {
                std::string extension = pattern;
                if (extension.starts_with("*.")) {
                    extension.erase(0, 2);
                } else if (extension.starts_with('*') || extension.starts_with('.')) {
                    extension.erase(0, 1);
                }
                if (extension.empty() || extension == "*") {
                    continue;
                }
                if (!spec.empty()) {
                    spec.push_back(',');
                }
                spec += extension;
            }
            return spec;
        }

        const char *OrNull(const std::string &value) {
            return value.empty() ? nullptr : value.c_str();
        }
    }

    std::optional<std::string> PickDirectory(const std::string &title, const std::string &defaultPath) {
        if (!DialogsReady()) {
            return std::nullopt;
        }

        const std::string directory = ExistingDirectoryHint(defaultPath);
        nfdpickfolderu8args_t args = {};
        args.defaultPath = OrNull(directory);
        args.title = OrNull(title);
        nfdu8char_t *path = nullptr;
        const nfdresult_t result = NFD_PickFolderU8_With(&path, &args);
        return TakePath(result, path);
    }

    std::optional<std::string> SaveFile(
        const std::string &title,
        const std::string &defaultPath,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns
    ) {
        if (!DialogsReady()) {
            return std::nullopt;
        }

        const DialogStart start = SplitStart(defaultPath);
        const std::string spec = FilterSpec(patterns);
        nfdu8filteritem_t filter = {};
        filter.name = filterDescription.empty() ? spec.c_str() : filterDescription.c_str();
        filter.spec = spec.c_str();
        nfdsavedialogu8args_t args = {};
        args.filterList = spec.empty() ? nullptr : &filter;
        args.filterCount = spec.empty() ? 0 : 1;
        args.defaultPath = OrNull(start.Directory);
        args.defaultName = OrNull(start.Name);
        args.title = OrNull(title);
        nfdu8char_t *path = nullptr;
        const nfdresult_t result = NFD_SaveDialogU8_With(&path, &args);
        return TakePath(result, path);
    }

    std::optional<std::string> PickFile(
        const std::string &title,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns,
        const std::string &defaultPath
    ) {
        if (!DialogsReady()) {
            return std::nullopt;
        }

        const DialogStart start = SplitStart(defaultPath);
        const std::string spec = FilterSpec(patterns);
        nfdu8filteritem_t filter = {};
        filter.name = filterDescription.empty() ? spec.c_str() : filterDescription.c_str();
        filter.spec = spec.c_str();
        nfdopendialogu8args_t args = {};
        args.filterList = spec.empty() ? nullptr : &filter;
        args.filterCount = spec.empty() ? 0 : 1;
        args.defaultPath = OrNull(start.Directory);
        args.title = OrNull(title);
        nfdu8char_t *path = nullptr;
        const nfdresult_t result = NFD_OpenDialogU8_With(&path, &args);
        return TakePath(result, path);
    }
}

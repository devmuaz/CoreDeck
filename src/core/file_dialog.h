//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#ifndef COREDECK_FILE_DIALOG_H
#define COREDECK_FILE_DIALOG_H

#include <optional>
#include <string>
#include <vector>

namespace CoreDeck::FileDialog {
    std::optional<std::string> PickDirectory(const std::string &title, const std::string &defaultPath = "");

    std::optional<std::string> SaveFile(
        const std::string &title,
        const std::string &defaultPath,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns
    );

    std::optional<std::string> PickFile(
        const std::string &title,
        const std::string &filterDescription,
        const std::vector<std::string> &patterns,
        const std::string &defaultPath = ""
    );
}

#endif // COREDECK_FILE_DIALOG_H
